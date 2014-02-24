#include <nopoll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#define CMD_SIZE 4

bool fileIncome = false;
uint32_t size_count_total = 0;
uint32_t msg_size_total = 0;
char *fName;
char *zsPath;

/**
  *
  *  This function checks, if the connection is established correctly. If true, the connection is finally established.
  *
  **/
nopoll_bool on_connection_opened (noPollCtx * ctx, noPollConn * conn, noPollPtr user_data)
{
    if (! nopoll_conn_set_sock_block (nopoll_conn_socket (conn), nopoll_false)) {
         printf ("ERROR: failed to configure non-blocking state to connection..\n");
         return nopoll_false;
    } 
            
    /* check to reject */
    if (nopoll_cmp (nopoll_conn_get_origin (conn), "http://deny.aspl.es"))  {
        printf ("INFO: rejected connection from %s, with Host: %s and Origin: %s\n",
                    nopoll_conn_host (conn), nopoll_conn_get_host_header (conn), nopoll_conn_get_origin (conn));
        return nopoll_false;
    } 

    /* notify connection accepted */
    /* printf ("INFO: connection received from %s, with Host: %s and Origin: %s\n"
                   nopoll_conn_host (conn), nopoll_conn_get_host_header (conn), nopoll_conn_get_origin (conn)); */
    printf ("Connection established!\n");
    return nopoll_true;
}

void deleteMsg (char *path_to_file) 
{
    if (remove(path_to_file) < 0) {
        printf("ERROR: An error occured, while deleting the file %s \n", path_to_file);
        return;
    }
}
 
void handleMsg (noPollCtx *ctx, noPollConn *con, noPollMsg *msg, noPollPtr *user_data)
{
    unsigned long msg_size = nopoll_msg_get_payload_size(msg);
    uint8_t *clientMsg = calloc (msg_size, sizeof(uint8_t)); 
    clientMsg = (uint8_t *) nopoll_msg_get_payload(msg);
 
    if (!fileIncome) {
        printf("Server received (size: %d)\n", nopoll_msg_get_payload_size(msg));
        /* Check if the msg is the initializing CMD msg before the file msgs*/
        char tmp[CMD_SIZE];
        fName = malloc((msg_size-CMD_SIZE)*sizeof(char));
        strncpy (tmp, clientMsg, 3);
        tmp[3] = '\0';
        
        printf("Server received msg: %s \n", (char *)clientMsg);
        if (strcmp(tmp, "ZSF") == 0) { // if the cmd prefix is set in the msg, the following chars are the Name of the incoming file
            if (strncmp(clientMsg, "ZSFDEL", 6) == 0) {
                char *fName_toDelete = malloc(((msg_size-(CMD_SIZE +3))* sizeof(char)));
                printf("Delete triggered!\n");
                
                int i;
                for (i = 0; i < msg_size; i++){
                    fName_toDelete[i] = clientMsg[i+(CMD_SIZE-1+3)]; // fill the filePath in the fName_toDelete string
                }
                fName_toDelete[strlen(fName_toDelete)] = '\0';
              
                printf("deleting...%s \n", fName_toDelete); 
                deleteMsg(fName_toDelete); 
                free(fName_toDelete);
                return;
            }
            fileIncome = true;
            
            int y;
            // get uint32 from network byte order
            msg_size_total = ((uint32_t) (clientMsg [3]) << 24)
                  + ((uint32_t) (clientMsg [4]) << 16)
                  + ((uint32_t) (clientMsg [5]) << 8)
                  +  (uint32_t) (clientMsg [6]);
            for (y = 0; y < msg_size; y++){
                fName[y] = clientMsg[y+(CMD_SIZE+4-1)]; // fill the fileName in the fName string
            }
            printf("msg_size_total: %"PRIu32"\n" , msg_size_total);
            fName[strlen(fName)] = '\0';
        }/* End of checking */

    } else {
        printf("Server received (size: %d)\n", nopoll_msg_get_payload_size(msg));
        printf("msg_size_total: %"PRIu32" size_count_total: %"PRIu32"\n" , msg_size_total, size_count_total);
        char *path = zsPath;
        char *fullPath;
        fullPath = malloc((strlen(fName)+strlen(path) + 1)*sizeof(char));
        strcpy(fullPath, path);
        strcat(fullPath, fName);
       
        printf("path: %s \n",fullPath); 
        FILE *file = fopen(fullPath, "a+b");
        if (file == NULL){
            printf("ERROR: Could not open the file to write!\n");
            return;
        }
        int z;
        for (z=0; z<msg_size; z++) {
            if ((fwrite(&clientMsg[z],(size_t)1,(size_t)1,file))<0) {
                printf("ERROR: Could not write to the file!\n");
                break;
            }
            size_count_total++;
            if (size_count_total == msg_size_total) {
                printf ("File end reached.\n");
                free (fName);
                fileIncome = false; //finished file - grant access the next file
                msg_size_total = 0;
                size_count_total = 0;
                break;
            }
        }
        fclose (file);
        free (fullPath);
    }
     
}

int main (int argc, char **argv) 
{
    if (argc != 2) {
        printf("Sorry, the Server has to have at least 1 Argument! Retry please! \n");
        return EXIT_FAILURE;
    }

    printf("Server started!\n");
    printf("Argv[1] %s \n", argv[1]);
    zsPath = argv[1];
    strcat(zsPath, "/");

    noPollCtx *ctx = nopoll_ctx_new();
    if (!ctx){
        printf("Could not create context. Shutting down!\n");
        return EXIT_FAILURE;
    } 

    // NULL ist the default port: 80
    noPollConn *listener = nopoll_listener_new(ctx, "localhost", "1234");
    if (!nopoll_conn_is_ok(listener))  {
        printf("The listener wasn´t initilialized correctly.\n");
        return EXIT_FAILURE;
    }  
    nopoll_ctx_set_on_msg(ctx, (noPollOnMessageHandler)handleMsg, NULL);
    nopoll_ctx_set_on_open(ctx, on_connection_opened, NULL);

    // Wait for Client Action
    nopoll_loop_wait(ctx, 0);

    printf("Close connection...\n");
    nopoll_conn_close(listener);
    nopoll_ctx_unref(ctx);
    return EXIT_SUCCESS;
}
