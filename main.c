#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <png.h>

int x, y;
int width, height;
png_byte color_type;
png_byte bit_depth;
png_structp png_ptr;
png_infop info_ptr;
int number_of_passes;
png_bytep *row_pointers;
__uint8_t *parsed_message;

//first twelve number of recaman sequence, to encode the length of text to png, 12 bit is enough for now, i guess..
size_t first_twelve[]={1,3,6,2,7,13,20,12,21,11,22,10};


void abort_(const char *s, ...)
{
        va_list args;
        va_start(args, s);
        vfprintf(stderr, s, args);
        fprintf(stderr, "\n");
        va_end(args);
        abort();
}

void read_png_file(char *file_name)
{
        char header[8]; // 8 is the maximum size that can be checked

        // open file and test for it being a png 
        FILE *fp = fopen(file_name, "rb");
        if (!fp)
                abort_("[read_png_file] File %s could not be opened for reading", file_name);
        fread(header, 1, 8, fp);
        if (png_sig_cmp(header, 0, 8))
                abort_("[read_png_file] File %s is not recognized as a PNG file", file_name);

        
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr)
                abort_("[read_png_file] png_create_read_struct failed");

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr)
                abort_("[read_png_file] png_create_info_struct failed");

        if (setjmp(png_jmpbuf(png_ptr)))
                abort_("[read_png_file] Error during init_io");

        png_init_io(png_ptr, fp);
        png_set_sig_bytes(png_ptr, 8);

        png_read_info(png_ptr, info_ptr);

        width = png_get_image_width(png_ptr, info_ptr);
        height = png_get_image_height(png_ptr, info_ptr);
        color_type = png_get_color_type(png_ptr, info_ptr);
        bit_depth = png_get_bit_depth(png_ptr, info_ptr);

        number_of_passes = png_set_interlace_handling(png_ptr);
        png_read_update_info(png_ptr, info_ptr);

        
        if (setjmp(png_jmpbuf(png_ptr)))
                abort_("[read_png_file] Error during read_image");

        row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
        for (y = 0; y < height; y++)
                row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));

        png_read_image(png_ptr, row_pointers);

        fclose(fp);
}

void write_png_file(char *file_name)
{
        
        FILE *fp = fopen(file_name, "wb");
        if (!fp)
                abort_("[write_png_file] File %s could not be opened for writing", file_name);

        
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr)
                abort_("[write_png_file] png_create_write_struct failed");

        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr)
                abort_("[write_png_file] png_create_info_struct failed");

        if (setjmp(png_jmpbuf(png_ptr)))
                abort_("[write_png_file] Error during init_io");

        png_init_io(png_ptr, fp);

        
        if (setjmp(png_jmpbuf(png_ptr)))
                abort_("[write_png_file] Error during writing header");

        png_set_IHDR(png_ptr, info_ptr, width, height,
                     bit_depth, color_type, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        png_write_info(png_ptr, info_ptr);

        
        if (setjmp(png_jmpbuf(png_ptr)))
                abort_("[write_png_file] Error during writing bytes");

        png_write_image(png_ptr, row_pointers);

        
        if (setjmp(png_jmpbuf(png_ptr)))
                abort_("[write_png_file] Error during end of write");

        png_write_end(png_ptr, NULL);

        
        for (y = 0; y < height; y++)
                free(row_pointers[y]);
        free(row_pointers);

        fclose(fp);
}


//to return the length from png file
unsigned int get_length_file(){
        //to read a pixel
        png_byte *row = row_pointers[0];
        png_byte *pixel;

        // variables needed for gathering length
        //first 12 number of recaman sequence. hardcoded due to these pixel holds the message length.
        size_t first_twelve[]={1,3,6,2,7,13,20,12,21,11,22,10}; 
        size_t aux_counter=0;//auxiliary counter, counts the ite. for the above array

        //temporary variables used for merging the bits together into an integer
        size_t tmp_counter =0; 
        __uint8_t tmp[12];
        __uint8_t temp=0;

        //gathering length
        for (size_t i = 1; i < 13; i++){
                pixel = &(row[first_twelve[aux_counter] * 4]); //first pixel
                temp|=pixel[0] & 0x03U;  
                //merge
                if(i%4== 0){
                        tmp[tmp_counter]=temp;
                        tmp_counter++;
                        temp=0;
                }
                temp = temp << 2;
                aux_counter++;
        }
        //number of characters multiplied with 4
        return (unsigned int)  ((tmp[0] << 16) | (tmp[1] << 8) | (tmp[2] ) ) * 4 ;
}

//to return the length from parsed message
unsigned int get_length_parsed(){
        // variables needed for gathering length 
        
        size_t aux_counter=0;
        size_t tmp_counter =0;
        __uint8_t tmp[12];
        __uint8_t temp=0;

        //gathering length
        for (size_t i = 1; i < 13; i++){
                temp|=parsed_message[i-1] & 0x03U;
                //merge
                if(i%4== 0){
                        tmp[tmp_counter]=temp;
                        tmp_counter++;
                        temp=0;
                }
                temp = temp << 2;
                aux_counter++;
        }
        //number of characters multiplied with 4
        return (unsigned int)  ((tmp[0] << 16) | (tmp[1] << 8) | (tmp[2] ) ) * 4 ;
}

//parse a message into smaller pieces to embed into png file
void parse_message(char *message)
{
        // resulting tokens are 4 times the number of chars in the message.
        //  can be returned as uint8_t;
        // every letter parsed into 4, resulting 4 different uint8_t and only lsb 2 are set(corresponds to letters bits).
        size_t index_counter = 12;
        
        parsed_message = malloc(((strlen(message) * 4) + 12 + 4) * sizeof(__uint8_t));
        
        // first two index of this array contains length, due to encoder function can accept a pointer in __uint8_t
        unsigned int length = strlen(message) + 4; // additional 4 null byte
        
        size_t aux_index =0;
        __uint8_t tmp[3];
        tmp[0] = (__uint8_t)((length & 0x00FF0000U) >> 16);
        tmp[1] = (__uint8_t)((length & 0x0000FF00U) >> 8);
        tmp[2] = (__uint8_t)(length & 0x000000FFU);

        //first 12 element will contain the number of elements in parsed_message
        for (size_t i = 0; i < 12; i+=4)
        {
                parsed_message[i] =   ((tmp[aux_index]& 0xC0) >> 6);
                parsed_message[i+1] = ((tmp[aux_index]& 0x30) >> 4);
                parsed_message[i+2] = ((tmp[aux_index]& 0x0C) >> 2);
                parsed_message[i+3] = ((tmp[aux_index]& 0x03));
                aux_index++;
        }
        for (size_t i = 0; i < 12; i++)
        {
                //printf("12number:%x\n",parsed_message[i]);
        }
        
        for (size_t i = 0; i < strlen(message); i++)
        {
                parsed_message[index_counter] = (((__uint8_t)message[i]) & 0xC0) >> 6;     // 1100 0000
                parsed_message[index_counter + 1] = (((__uint8_t)message[i]) & 0x30) >> 4; // 0011 0000
                parsed_message[index_counter + 2] = (((__uint8_t)message[i]) & 0x0C) >> 2; // 0000 1100
                parsed_message[index_counter + 3] = ((__uint8_t)message[i]) & 0x03;        // 0000 0011
                // printf("%X, %X, %X, %X\n", parsed_message[index_counter],parsed_message[index_counter+1],parsed_message[index_counter+2],parsed_message[index_counter+3]);
                index_counter += 4;
        }
}


void encode(char* infile, char* outfile, char* message)
{
        read_png_file(infile);
        parse_message(message);
        // number of pixels that are going to be altered
        // this number needed to be extracted from parsed_message, unlike get_length extracts it from raw image
        unsigned int number_of_pixel=get_length_parsed();
        //printf("encode,length:%d\n",number_of_pixel);
        
        size_t last_alloceted = 1; // amount of allocated space counter
        int generated_number = 0;  // the recaman-genarated number
        //lookup table
        int *used = (int *)malloc(sizeof(int) * last_alloceted);
        used[0] = 0;

        size_t x;
        size_t y;
        unsigned int succesfull_write = 0; // number of successfull write to count the iteration of message that will be embedded
        bool write_permission = true;
        int i = 1; // recamans iterator
        #ifdef DEBUG
            // opening file with read+update permissions
            FILE *logfile = fopen("w_data.txt", "w");
        #endif
        // recaman number genereating loop start
        while (succesfull_write < number_of_pixel)
        {
                write_permission = 1;
                if (generated_number - i > 0 && used[generated_number - i] != generated_number - i)
                {
                        generated_number -= i;
                        used[generated_number] = generated_number;
                }
                else
                {
                        // fprintf(logfile, "before:%d,\t",generated_number,i);
                        generated_number += i;
                        // fprintf(logfile, "after:%d,\ti:%d\n",generated_number,i);
                        if (last_alloceted <= generated_number)
                        {
                                used = realloc(used, (size_t)sizeof(int) * (i + last_alloceted));
                                last_alloceted += i;
                        }
                        if (used[generated_number] == generated_number)
                        {
                                //  will bool a var to false  to prevent pixel encoding due to this number 
                                //being generated for more than one time.
                                write_permission = 0;
                        }
                        used[generated_number] = generated_number;
                } // recaman number generating loop end

                // recaman squence actually repeats itself, means generated number migth (will) be generated again.
                // one way to overcome this would be using the lookup table(used[]) to check if that number ever generated
                // if not, use that number to address a pixel and embed the data into it.

                if (write_permission != 0)
                {
                        x = generated_number % width;
                        y = (size_t)generated_number / width; // casting it to int  is enough to round it to lower bound.
                        png_byte *row = row_pointers[y];
                        png_byte *ptr = &(row[x * 4]);
                        //fprintf(logfile, "before:%X\t", ptr[0]);
                        // only red pixel gets altered
                        ptr[0] &= 0xFC; // 0xFC -> 0b11111100 (to make it visible: 0x3F->00111111)
                        ptr[0] |= parsed_message[succesfull_write];
                        #ifdef DEBUG
                            fprintf(logfile, "recaman:%d\t\trecaman index:%d\t\twrite_data[succ]:%X\t\tsucc:%d\n",generated_number,i,parsed_message[succesfull_write], succesfull_write);
                        #endif
                        ++succesfull_write;
                }
                i++;
        }
        // free(message);
        free(used);
        #ifdef DEBUG
            fclose(logfile);
        #endif
        write_png_file(outfile);
}

char *decode(char *message, char* outfile)
{
        read_png_file(outfile);
        //recaman-necessary vars
        size_t last_alloceted = 25; // amount of allocated space counter
        int generated_number = 10;  // the recaman-genarated number
        // allocate mem for lookup table
        int *used = (int *)malloc(sizeof(int) * last_alloceted);
        used[0] = 0;
        // number of pixels that are going to be read
        unsigned int number_of_pixel = (get_length_file()-16);
        

        //first 12 element holds the data length and we get that value with get_length_file().
        //but not filling these pixels in lookup table  after reading, recaman number generation somehow reaches 
        //those pixels due to first 12 pixel is not marked in lookup table. so we do that in here.
        for (size_t i = 0; i < 12; i++)
        {
                used[first_twelve[i]]=first_twelve[i];
        }
        
        //array to hold the last two bits that are extracted from pixels, determined by recaman-generated number
        __uint8_t *read_data = malloc(number_of_pixel * sizeof(__uint8_t));
        size_t read_data_counter=0;
        unsigned char character;// temporary reading iterator

        size_t x;
        size_t y;
        unsigned int succesfull_write = 0; // number of successfull write. to eliminate the recaman-sequence's culprit of regenrating same number.
        bool read_permission = true; // to deny or pass of reading.
        int i = 13; // recamans iterator. starts from 13 ,as first 12 item holds the data of  number of pixeles inside the file.

        // opening file with write permissions
        #ifdef DEBUG
            FILE *logfile = fopen("r_data.txt", "w");
        #endif

        // recaman number genereating loop start
        while (succesfull_write < number_of_pixel)
        {
                read_permission = 1;
                if (generated_number - i > 0 && used[generated_number - i] != generated_number - i)
                {
                        generated_number -= i;
                        used[generated_number] = generated_number;
                }
                else
                {
                        generated_number += i;       
                        if (last_alloceted <= generated_number)
                        {
                                used = realloc(used, (size_t)sizeof(int) * (i + last_alloceted));
                                last_alloceted += i;
                        }
                        if (used[generated_number] == generated_number)
                        {
                                //  will set the  var to  false  to prevent pixel decoding.
                                // we are here because the last number generated is generated before and reading will be denied.
                                read_permission = 0;
                        }
                        used[generated_number] = generated_number;
                } // recaman number generating loop end

                
                if (read_permission != 0)
                {
                        x = generated_number % width;
                        y = (size_t)generated_number / width; // casting it to int  is enough to round it to lower bound.

                        png_byte *row = row_pointers[y];
                        png_byte *ptr = &(row[x * 4]);

                        read_data[read_data_counter] = (ptr[0] & 0x03U);
                        
                        #ifdef DEBUG
                            fprintf(logfile, "recaman:%d\t\trecaman index:%d\t\tread_data[succ]:%X\t\tsucc:%d\n",generated_number,i,read_data[succesfull_write], succesfull_write);
                        #endif
                        ++succesfull_write;
                        ++read_data_counter;
                }
                i++;
        }

        free(used); // no need to keep the lookup table 

        //now will merge every 4 data (which every data is saved as last 2 bits) into a char and save it into message
        size_t message_counter=0;
        message = malloc((number_of_pixel/4)*sizeof(unsigned char));
        FILE *result = fopen("result.txt","w");
        //get chars out of parsed_data
        for (size_t i = 1; i < succesfull_write+1; i++)
        {
                character|=(read_data[i-1] & 0x03U);
                
                //merge
                if(i%4== 0){
                        message[message_counter]=character;
                        message_counter++;
                        character='\0';
                }
                character = character << 2;
        }
        
        for (size_t i = 0; i < number_of_pixel/4; i++)
        {
               fprintf(result,"%c",message[i]);        
        }
        
        #ifdef DEBUG
            fclose(logfile);
        #endif
        return message;
}


int main(int argc, char** argv){
    
    //usage:  $ ./<program_name> <textfile> <input_png_file_to_encode_text_into> <output_png_file>
    
    //reading text file
    char *message = malloc(sizeof(size_t) * 11085);
    FILE *textfile = fopen(argv[1], "r");
    fgets(message, 11080, textfile);
    
    
    char *decoded_message;
    
    //encoed the text into
    encode(argv[2], argv[3], message);
    
    //decode text from png
    decoded_message = decode(decoded_message, argv[3]);

    return 0;
}

