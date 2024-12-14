#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <pthread.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")  // Link Winsock library

#define PORT 6378
#define MAX_CLIENTS 5
#define WORD_SIZE 5
#define BUFFER_SIZE 1024

// Sample words for the game
char *words[] = {"apple", "grape", "peach", "lemon", "mango"};
int word_count = 5;

typedef struct {
    SOCKET socket;
    int score;
    char username[50];
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
char secret_word[WORD_SIZE + 1];

// Function to select a random word
void select_random_word() {
    srand(time(NULL));
    int random_index = rand() % word_count +1 ;  // Pick a random index
    strcpy(secret_word, words[random_index]);
    printf("Secret word for this round: %s\n", secret_word);  // For debugging
}

// Function to handle each client
DWORD WINAPI handle_client(LPVOID arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    int guess_count = 0;

    send(client->socket, "Welcome to Wordle! Guess the 5-letter word.\n", strlen("Welcome to Wordle! Guess the 5-letter word.\n"), 0);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client->socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Client disconnected.\n");
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        printf("Received guess from %s: %s\n", client->username, buffer);

        if (strlen(buffer) != WORD_SIZE) {
            send(client->socket, "Guess must be 5 letters.\n", strlen("Guess must be 5 letters.\n"), 0);
            continue;
        }

        char feedback[WORD_SIZE + 1];
        for (int i = 0; i < WORD_SIZE; i++) {
            feedback[i] = (buffer[i] == secret_word[i]) ? buffer[i] : '_';
        }
        feedback[WORD_SIZE] = '\0';

        guess_count++;
        if (strcmp(buffer, secret_word) == 0) {
            client->score += 10 - guess_count;
            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "Correct! You've guessed the word '%s' in %d attempts. Your score: %d\n", secret_word, guess_count, client->score);
            send(client->socket, msg, strlen(msg), 0);

            select_random_word();
            guess_count = 0;
        } else {
            char msg[BUFFER_SIZE];
            snprintf(msg, sizeof(msg), "Feedback: %s\n", feedback);
            send(client->socket, msg, strlen(msg), 0);
        }
    }

    closesocket(client->socket);
    return 0;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Winsock initialization failed\n");
        return 1;
    }

    SOCKET server_socket, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, MAX_CLIENTS) == SOCKET_ERROR) {
        printf("Listen failed\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    printf("Server is listening on port %d...\n", PORT);
    select_random_word();

    while (1) {
        new_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen);
        if (new_socket == INVALID_SOCKET) {
            printf("Accept failed\n");
            continue;
        }

        if (client_count >= MAX_CLIENTS) {
            printf("Max clients reached. Rejecting connection.\n");
            closesocket(new_socket);
            continue;
        }

        Client *client = &clients[client_count++];
        client->socket = new_socket;
        client->score = 0;
        snprintf(client->username, sizeof(client->username), "Player%d", client_count);

        printf("%s connected.\n", client->username);

        DWORD thread_id;
        HANDLE thread_handle = CreateThread(NULL, 0, handle_client, (void *)client, 0, &thread_id);
        CloseHandle(thread_handle);
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}
