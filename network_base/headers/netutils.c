#include "netutils.h"

void fetch_and_print_error(int socket)
{
  int payload_size;
  u_char *payload;
  recv_int_value_socket(socket, &payload_size);

  recv_from_socket(socket, payload, payload_size);
  payload[payload_size] = NULL_CHAR;
  printf("<<< Error Message: %s\n", (char *)payload);
  free(payload);
}
void send_int_value_socket(int socket, int value)
{
  u_char payload[INT_SIZE];
  encode_int_to_uchar(payload, value);
  send_to_socket(socket, payload, INT_SIZE);
}

void recv_int_value_socket(int socket, int *value)
{
  u_char payload[INT_SIZE];
  recv_from_socket(socket, payload, INT_SIZE);
  decode_int_from_uchar(payload, value);
}

int send_to_socket(int socket, u_char *payload, int size_of_payload)
{
  int s_bytes = 0;

  while (s_bytes != size_of_payload)
  {
    if ((s_bytes += send(socket, payload + s_bytes, size_of_payload - s_bytes, 0)) < 0)
    {
      perror("Unable to send entire payload via socket");
      exit(1);
    }
  }

  return s_bytes;
}

void send_signal(int *conn_fds, int conn_count, u_char signal)
{
  int i;
  for (i = 0; i < conn_count; i++)
  {
    if (conn_fds[i] == -1)
      continue;
    send_to_socket(conn_fds[i], &signal, sizeof(u_char));
  }
}

void recv_signal(int socket, u_char *payload)
{
  recv_from_socket(socket, payload, sizeof(u_char));
}

int recv_from_socket(int socket, u_char *payload, int size_of_payload)
{
  int r_bytes = 0;

  while (r_bytes != size_of_payload)
  {
    if ((r_bytes += recv(socket, payload + r_bytes, size_of_payload - r_bytes, 0)) < 0)
      exit(1);

    if (r_bytes == 0)
      break;
  }
  return r_bytes;
}
int encode_user_struct(char *buffer, user_struct *user)
{
  int n_bytes;
  n_bytes = sprintf(buffer, AUTH_TEMPLATE, AUTH_FLAG, user->username, user->password);
  if (n_bytes < 0)
  {
    exit(1);
  }
  return n_bytes;
}

void decode_user_struct(char *buffer, user_struct *user)
{
  int flag;
  if ((sscanf(buffer, AUTH_TEMPLATE, &flag, user->username, user->password) <= 0))
    exit(1);

  if (flag != AUTH_FLAG)
    exit(1);
}

void encode_int_to_uchar(u_char *buffer, int n)
{
  buffer[0] = (n >> 24) & 0xFF;
  buffer[1] = (n >> 16) & 0xFF;
  buffer[2] = (n >> 8) & 0xFF;
  buffer[3] = n & 0xFF;
}

void decode_int_from_uchar(u_char *buffer, int *n)
{
  int temp;

  temp = buffer[0] << 24;
  temp |= buffer[1] << 16;
  temp |= buffer[2] << 8;
  temp |= buffer[3];

  *n = temp;
}

void encode_server_chunks_info_struct_to_buffer(u_char *buffer, server_chunks_info_struct *server_chunks_info)
{
  int i;
  encode_int_to_uchar(buffer, server_chunks_info->chunks);

  for (i = 0; i < server_chunks_info->chunks; i++)
  {
    encode_chunk_info_struct_to_buffer(buffer + INT_SIZE + i * CHUNK_INFO_STRUCT_SIZE, &server_chunks_info->chunk_info[i]);
  }
}

void decode_server_chunks_info_struct_from_buffer(u_char *buffer, server_chunks_info_struct *server_chunks_info)
{
  int i;
  decode_int_from_uchar(buffer, &server_chunks_info->chunks);

  for (i = 0; i < server_chunks_info->chunks; i++)
  {
    decode_chunk_info_struct_from_buffer(buffer + INT_SIZE + i * CHUNK_INFO_STRUCT_SIZE, &server_chunks_info->chunk_info[i]);
  }
}

void encode_chunk_info_struct_to_buffer(u_char *buffer, chunk_info_struct *chunk_info)
{
  int i;
  memcpy(buffer, chunk_info->file_name, MAXCHARBUFF);
  for (i = 0; i < CHUNKS_PER_SERVER; i++)
    encode_int_to_uchar(buffer + MAXCHARBUFF + i * INT_SIZE, chunk_info->chunks[i]);
}

void decode_chunk_info_struct_from_buffer(u_char *buffer, chunk_info_struct *chunk_info)
{
  int i;
  memcpy(chunk_info->file_name, buffer, MAXCHARBUFF);

  for (i = 0; i < CHUNKS_PER_SERVER; i++)
    decode_int_from_uchar(buffer + MAXCHARBUFF + i * INT_SIZE, &chunk_info->chunks[i]);
}

void write_split_to_socket_as_stream(int socket, split_struct *split)
{
  u_char payload_buffer[MAX_SEG_SIZE];
  int content_bytes_sent, bytes_to_send_next;

  memset(payload_buffer, 0, sizeof(payload_buffer));
  payload_buffer[0] = INITIAL_WRITE_FLAG;
  encode_int_to_uchar(payload_buffer + 1, split->id);
  encode_int_to_uchar(payload_buffer + 5, split->content_length);
  bytes_to_send_next = split->content_length;

  bytes_to_send_next = (bytes_to_send_next < MAX_SEG_SIZE - 9) ? bytes_to_send_next : MAX_SEG_SIZE - 9;
  bcopy(split->content, payload_buffer + 9, bytes_to_send_next);

  send_to_socket(socket, payload_buffer, MAX_SEG_SIZE);

  content_bytes_sent = bytes_to_send_next;
  if (split->content_length > MAX_SEG_SIZE - 9)
  {
    send_to_socket(socket, split->content + content_bytes_sent, split->content_length - content_bytes_sent);
  }
}

void write_split_from_socket_as_stream(int socket, split_struct *split)
{
  u_char payload_buffer[MAX_SEG_SIZE];
  int content_bytes_recv, bytes_to_recv_next;

  memset(payload_buffer, 0, MAX_SEG_SIZE * sizeof(u_char));

  recv_from_socket(socket, payload_buffer, MAX_SEG_SIZE);

  if (payload_buffer[0] == INITIAL_WRITE_FLAG)
  {

    decode_int_from_uchar(payload_buffer + 1, &split->id);
    decode_int_from_uchar(payload_buffer + 5, &split->content_length);

    bytes_to_recv_next = split->content_length;
    bytes_to_recv_next = (bytes_to_recv_next < MAX_SEG_SIZE - 9) ? bytes_to_recv_next : MAX_SEG_SIZE - 9;
    bcopy(payload_buffer + 9, split->content, bytes_to_recv_next);
    content_bytes_recv = bytes_to_recv_next;
  }

  if (split->content_length > content_bytes_recv)
  {

    recv_from_socket(socket, split->content + content_bytes_recv, split->content_length - content_bytes_recv);
  }
}

void encode_split_struct_to_buffer(u_char *buffer, split_struct *split)
{
  int i;
  u_char *ptr;

  encode_int_to_uchar(buffer + 1, split->id);
  encode_int_to_uchar(buffer + 5, split->content_length);
  ptr = &buffer[9];
  for (i = 0; i < split->content_length; i++)
    ptr[i] = split->content[i];
}

void decode_split_struct_from_buffer(u_char *buffer, split_struct *split)
{
  int i;
  u_char *ptr;
  fprintf(stderr, "First Byte %u\n", buffer[0]);
  if (buffer[0] == 0)
    buffer = &buffer[1];

  split->id = (int)buffer[0];
  split->content_length = (int)buffer[1];

  ptr = &buffer[2];
  for (i = 0; i < split->content_length; i++)
    split->content[i] = ptr[i];
}
