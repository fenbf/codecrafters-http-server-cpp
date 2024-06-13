#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

[[nodiscard]] std::string GetFileContents(const fs::path& filePath) {
    std::ifstream inFile{ filePath, std::ios::in | std::ios::binary };
    if (!inFile)
        throw std::runtime_error("Cannot open " + filePath.filename().string());

    std::string str(static_cast<size_t>(fs::file_size(filePath)), 0);

    inFile.read(str.data(), str.size());
    if (!inFile)
        throw std::runtime_error("Could not read the full contents from " + filePath.filename().string());

    return str;
}


void handle_client(int client_fd, std::string dir) {
    char buffer[255] { 0 };
    const int len = read(client_fd, buffer, 255);

    if (len < 0) {
        std::cout << "Failed to read the request\n";
        close(client_fd);
        return;
    }

    std::cout << "Read buffer:\n" << buffer << std::endl;

    std::string str {buffer};
    std::string response = "HTTP/1.1 404 Not Found\r\n\r\n";
  
    const std::string echo = "GET /echo/";
    auto pos = str.find(echo);
    if (pos == 0)
    {
        std::cout << "starts with echo...\n";
        auto spaceAfter = str.find(' ', echo.length());
        auto randomString = str.substr(echo.length(), spaceAfter - echo.length());
        std::cout << "string is: " << randomString << '\n';
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
        response += std::to_string(randomString.length()) + "\r\n\r\n" + randomString + "\r\n";
    }
    else
    {
        pos = str.find('/');  
        if (pos != std::string::npos)
        {
            std::cout << "pos: " << pos << '\n';
            if (str[pos+1] == ' ')
            {
                response = "HTTP/1.1 200 OK\r\n\r\n";
            }
            else 
            {
                pos = str.find("/user-agent");
                if (pos != std::string::npos)
                {
                    pos = str.find("User-Agent:");
                    if (pos != std::string::npos)
                    {              
                        auto endPos = str.find("\r\n", pos);
                        auto startPos = pos+std::string("User-Agent: ").length();
                        auto agent = str.substr(startPos, endPos - startPos);
                        std::cout << "agent:\"" << agent << "\", count: " << agent.length() << "\n";
                        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
                        response += std::to_string(agent.length()) + "\r\n\r\n" + agent + "\r\n";
                    }
                }
                else // files?
                {
                    pos = str.find("/files/");
                    if (pos != std::string::npos)
                    {
                        std::cout << "trying to load a file...\n";          
                        auto endPos = str.find(" ", pos);
                        auto startPos = pos+std::string("/files/").length();
                        auto fileName = str.substr(startPos, endPos - startPos);
                        std::cout << "file:\"" << fileName << "\"\n";
                        auto fullFile = dir + fileName;
                        std::cout << "full:\"" << fullFile << "\"\n";
                        if (std::filesystem::exists(fullFile))
                        {
                            std::cout << "exists!\n";
                            response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: ";
                            auto data = GetFileContents(fullFile);
                            response += std::to_string(data.length()) + "\r\n\r\n" + data + "\r\n";
                        }
                    }
                }
            }
        }
    }

    std::cout << "Response: " << response << '\n';

    if (write(client_fd, response.c_str(), response.length()) < 0) {
        std::cout << "Failed to write the request\n";
    }

    std::cout << "Message sent" << std::endl;
    close(client_fd);
}

int main(int argc, char **argv) {
    std::cout << "Logs from your program will appear here!\n";

    std::string dir;
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
            std::cout << i << " param: " << argv[i] << '\n';

        if (argc == 3 && strcmp(argv[1], "--directory") == 0)
        {
  	        dir = argv[2];
            std::cout << "got directory param: \"" << dir << "\"\n";
        }
    }
    else
        std::cout << "no params passed...\n";

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    std::cout << "Waiting for clients to connect...\n";

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_fd < 0) {
            std::cerr << "Failed to accept client connection\n";
            continue;
        }
        std::cout << "Client connected\n";

        std::jthread(handle_client, client_fd, dir);
    }

    close(server_fd);
    return 0;
}
