#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8085
#define BACKLOG 5
#define BUFFER_SIZE 1024

volatile sig_atomic_t sighupReceived = 0;

void sigHupHandler(int sigNumber) {
    sighupReceived = 1;
    printf("sigHubHandler acted\n");
}

void initServerSocket(int *serverFD, struct sockaddr_in *socketAddress) {
    // Socket creating
    if ((*serverFD = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("create error");
        exit(EXIT_FAILURE);
    }

    // Setting socket address parameters
    socketAddress->sin_family = AF_INET;
    socketAddress->sin_addr.s_addr = INADDR_ANY;
    socketAddress->sin_port = htons(PORT);

    // Socket binding to the address
    if (bind(*serverFD, (struct sockaddr*)socketAddress, sizeof(*socketAddress)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    // Started socket listening
    if (listen(*serverFD, BACKLOG) < 0) {
        perror("listen error");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d \n\n", PORT);
}

void signalBlocking(sigset_t blockedMask, sigset_t origMask) {
    sigemptyset(&blockedMask);
    sigemptyset(&origMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);
}

void setupSignalHandling() {
    struct sigaction sa;
    int sigactionFirst = sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    int sigactionSecond = sigaction(SIGHUP, &sa, NULL);
}

void handleConnection(int* incomingSocketFD) {
    char buffer[BUFFER_SIZE] = { 0 };
    int readBytes = read(*incomingSocketFD, buffer, BUFFER_SIZE);

    if (readBytes > 0) { 
        printf("Received data: %d bytes\n", readBytes);
        printf("Received message from client: %s\n", buffer);

        const char* response = "Hello from server!";
        if (send(*incomingSocketFD, response, strlen(response), 0) < 0) {
            perror("send error");
        }

    } else {
        if (readBytes == 0) {
            close(*incomingSocketFD); 
            *incomingSocketFD = 0; 
            printf("Connection closed\n\n");
        } else { 
            perror("read error"); 
        }  
    } 
}

int main() {
    int serverFD;
    int incomingSocketFD = 0; 
    struct sockaddr_in socketAddress; 
    int addressLength = sizeof(socketAddress);
    fd_set readfds;
    sigset_t blockedMask, origMask;
    int maxSd;

    // Initialize server socket
    initServerSocket(&serverFD, &socketAddress);

    // Setting up signal blocking
    signalBlocking(blockedMask, origMask);

    // Signal handler registration
    setupSignalHandling();

    while (1) {

        FD_ZERO(&readfds); 
        FD_SET(serverFD, &readfds); 
        
        if (incomingSocketFD > 0) { 
            FD_SET(incomingSocketFD, &readfds); 
        } 
        
        maxSd = (incomingSocketFD > serverFD) ? incomingSocketFD : serverFD; 
 
        if (pselect(maxSd + 1, &readfds, NULL, NULL, NULL, &origMask) != -1)
        {
            if (sighupReceived) {
                printf("SIGHUP received.\n");
                sighupReceived = 0;
                continue;
            }

            
        } else {
            if (errno != EINTR) {
                perror("pselect error"); 
                exit(EXIT_FAILURE); 
            }
        }

        // пселект должен вернуть не -1 только если не -1 есть мсысл проводить проверку фд, если -1 то errno == einter если не eintr то ошибка, 
    
        // Reading incoming bytes
        if (incomingSocketFD > 0 && FD_ISSET(incomingSocketFD, &readfds)) { 
            handleConnection(&incomingSocketFD);
            continue;
        }
        
        // Check of incoming connections
        if (FD_ISSET(serverFD, &readfds)) {
            if ((incomingSocketFD = accept(serverFD, (struct sockaddr*)&socketAddress, (socklen_t*)&addressLength)) < 0) {
                perror("accept error");
                exit(EXIT_FAILURE);
            }

            printf("New connection.\n");


        }

        
    }

    close(serverFD);

    return 0;
}
