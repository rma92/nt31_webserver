#include <windows.h>
#include <winsock.h>


/*
cl web_ws1_a.c /link /nodefaultlib /FIXED:NO /subsystem:windows,3.10 /entry:_start kernel32.lib user32.lib wsock32.lib /out:web1.exe
*/
#define PORT 9102
#define BACKLOG 10
#define BUFSIZE 1024

// File data
const char *filenames[] = {"/metrics"};
const char *filedata[] = {"server_requests_total 42\n"};

// A simple wrapper for sending strings, since we can't use printf
void send_string(SOCKET socket, const char *str) {
    send(socket, str, lstrlenA(str), 0);
}

void handle_client(SOCKET client_socket) {
    char buffer[BUFSIZE];
    int bytes_received = recv(client_socket, buffer, BUFSIZE - 1, 0);
    char method[16], path[256], protocol[16];
    int method_index = 0, path_index = 0, protocol_index = 0;
    int stage = 0;
    int i;
    BOOL found = FALSE;

    if (bytes_received == SOCKET_ERROR) {
        closesocket(client_socket);
        return;
    }

    buffer[bytes_received] = '\0'; // Null-terminate the request

    // Extract the HTTP request line

    for (i = 0; i < bytes_received; i++) {
        if (buffer[i] == ' ' && stage < 2) {
            stage++;
        } else if (buffer[i] == '\r' || buffer[i] == '\n') {
            break;
        } else {
            if (stage == 0 && method_index < sizeof(method) - 1) {
                method[method_index++] = buffer[i];
            } else if (stage == 1 && path_index < sizeof(path) - 1) {
                path[path_index++] = buffer[i];
            } else if (stage == 2 && protocol_index < sizeof(protocol) - 1) {
                protocol[protocol_index++] = buffer[i];
            }
        }
    }

    method[method_index] = '\0';
    path[path_index] = '\0';
    protocol[protocol_index] = '\0';

    // Check the method
    if (lstrcmpA(method, "GET") != 0) {
        send_string(client_socket, "HTTP/1.1 405 Method Not Allowed\r\n\r\n");
        closesocket(client_socket);
        return;
    }

    // Match the path with filenames
    for (i = 0; i < sizeof(filenames) / sizeof(filenames[0]); i++) {
        if (lstrcmpA(path, filenames[i]) == 0) {
            found = TRUE;
            send_string(client_socket, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n");
            send_string(client_socket, filedata[i]);
            break;
        }
    }

    if (!found) {
        send_string(client_socket, "HTTP/1.1 404 Not Found\r\n\r\n");
    }

    closesocket(client_socket);
}

void _start() {
    WSADATA wsaData;
    SOCKET server_socket;
    struct sockaddr_in server_addr;
    fd_set read_fds;
    HANDLE hStdOut;

    hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    // Print a startup message
    if (hStdOut != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hStdOut, "init", 42, &written, NULL);
    }

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        ExitProcess(1);
    }

    // Create a socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        WSACleanup();
        ExitProcess(1);
    }

    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        closesocket(server_socket);
        WSACleanup();
        ExitProcess(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, BACKLOG) == SOCKET_ERROR) {
        closesocket(server_socket);
        WSACleanup();
        ExitProcess(1);
    }

    // Print a startup message
    if (hStdOut != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteConsoleA(hStdOut, "Server listening on http://127.0.0.1:9102\n", 42, &written, NULL);
    }

    // Main loop
    while (TRUE) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        // Wait for activity
        if (select(0, &read_fds, NULL, NULL, NULL) == SOCKET_ERROR) {
            break;
        }

        // Check for new connections
        if (FD_ISSET(server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            int addr_len = sizeof(client_addr);
            SOCKET client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);

            if (client_socket == INVALID_SOCKET) {
                continue;
            }

            handle_client(client_socket);
        }
    }

    // Cleanup
    closesocket(server_socket);
    WSACleanup();
    ExitProcess(0);
}

