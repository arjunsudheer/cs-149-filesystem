#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 64
#define MAX_NAME 32

typedef struct File
{
    char name[MAX_NAME];
    int startBlock;
    int size;
    int isOpen;
    char contents[BLOCK_SIZE];
} File;

typedef struct Directory
{
    char name[MAX_NAME];
    struct Directory *parent;
    struct Directory *subdirs[16];
    int subdirCount;
    File *files;
    int fileCount;
} Directory;

// Represents the hard disk storage
unsigned char *disk;
// Bitmap to keep track of free blocks
unsigned char *bitmap;
// Maximum number of files the disk can hold
int maxFiles;
// Root directory of the filesystem
Directory *root;

// Modifies the bitmap to mark a block as used in the disk array
int alloc_block()
{
    for (int i = 0; i < maxFiles; i++)
    {
        if (bitmap[i] == 0)
        {
            bitmap[i] = 1;
            return i;
        }
    }
    return -1;
}

Directory *make_dir(char *name, Directory *parent)
{
    Directory *d = malloc(sizeof(Directory));
    strcpy(d->name, name);
    d->parent = parent;
    d->subdirCount = 0;
    d->fileCount = 0;
    d->files = malloc(sizeof(File) * maxFiles);
    return d;
}

File *find_file_in_dir(Directory *d, char *name)
{
    for (int i = 0; i < d->fileCount; i++)
        if (strcmp(d->files[i].name, name) == 0)
            return &d->files[i];
    return NULL;
}

Directory *find_subdir(Directory *d, char *name)
{
    for (int i = 0; i < d->subdirCount; i++)
        if (strcmp(d->subdirs[i]->name, name) == 0)
            return d->subdirs[i];
    return NULL;
}

// Finds a file in the entire disk regardless of the user's current path
File *find_file_global(Directory *d, char *name, Directory **foundIn)
{
    File *f = find_file_in_dir(d, name);
    if (f)
    {
        if (foundIn)
            *foundIn = d;
        return f;
    }
    for (int i = 0; i < d->subdirCount; i++)
    {
        f = find_file_global(d->subdirs[i], name, foundIn);
        if (f)
            return f;
    }
    return NULL;
}

void create_file(Directory *cwd)
{
    char name[MAX_NAME];
    printf("Enter file name: ");
    scanf("%s", name);

    if (find_file_in_dir(cwd, name))
    {
        printf("Error: File exists.\n");
        return;
    }

    int block = alloc_block();
    if (block < 0)
    {
        printf("Error: Disk full.\n");
        return;
    }

    File *f = &cwd->files[cwd->fileCount++];
    strcpy(f->name, name);
    f->startBlock = block;
    f->size = 0;
    f->isOpen = 0;
    memset(f->contents, 0, BLOCK_SIZE);

    printf("File '%s' created at block %d.\n", name, block);
}

void open_file()
{
    char name[MAX_NAME];
    printf("Enter file name: ");
    scanf("%s", name);

    File *f = find_file_global(root, name, NULL);
    if (!f)
    {
        printf("Error: File not found.\n");
        return;
    }

    // Load from simulated disk to buffer
    memcpy(f->contents, &disk[f->startBlock * BLOCK_SIZE], BLOCK_SIZE);
    f->isOpen = 1;

    printf("Editing file %s\n", f->name);
    printf("Current content: %s\n", f->size > 0 ? f->contents : "[Empty]");
    printf("Enter new text (will be appended, type 'END' to finish):\n");

    char line[BLOCK_SIZE];
    while (1)
    {
        if (!fgets(line, BLOCK_SIZE, stdin))
            break;
        if (strncmp(line, "END", 3) == 0)
            break;

        if (strlen(f->contents) + strlen(line) < BLOCK_SIZE)
        {
            strcat(f->contents, line);
        }
        else
        {
            printf("Buffer full, truncating file contents\n");
            break;
        }
    }

    f->size = strlen(f->contents);
    printf("Buffer updated. Close file to save changes to disk.\n");
}

void close_file()
{
    char name[MAX_NAME];
    printf("Enter file name: ");
    scanf("%s", name);

    File *f = find_file_global(root, name, NULL);
    if (!f || !f->isOpen)
    {
        printf("Error: File not found or not open.\n");
        return;
    }

    // Save from buffer to simulated disk
    memcpy(&disk[f->startBlock * BLOCK_SIZE], f->contents, BLOCK_SIZE);
    f->isOpen = 0;
    printf("File %s saved to disk and closed.\n", f->name);
}

void create_dir(Directory *cwd)
{
    char name[MAX_NAME];
    printf("Enter directory name: ");
    scanf("%s", name);

    if (find_subdir(cwd, name))
    {
        printf("Error: Directory exists.\n");
        return;
    }

    if (cwd->subdirCount >= 16)
    {
        printf("Error: Directory full.\n");
        return;
    }

    cwd->subdirs[cwd->subdirCount++] = make_dir(name, cwd);
    printf("Directory %s created.\n", name);
}

void search_file()
{
    char name[MAX_NAME];
    printf("Enter file name to search: ");
    scanf("%s", name);

    Directory *foundIn = NULL;
    File *f = find_file_global(root, name, &foundIn);

    if (f)
    {
        printf("Found file %s in directory %s\n", f->name, foundIn->name);
    }
    else
    {
        printf("File %s not found\n", name);
    }
}

void print_tree(Directory *d, int level)
{
    for (int i = 0; i < level; i++)
        printf("  ");
    printf("[%s]\n", d->name);

    for (int i = 0; i < d->fileCount; i++)
    {
        for (int j = 0; j < level + 1; j++)
            printf("  ");
        printf("- %s (%d bytes)\n", d->files[i].name, d->files[i].size);
    }

    for (int i = 0; i < d->subdirCount; i++)
    {
        print_tree(d->subdirs[i], level + 1);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <max_files>\n", argv[0]);
        return 1;
    }

    maxFiles = atoi(argv[1]);
    disk = calloc(maxFiles, BLOCK_SIZE);
    bitmap = calloc(maxFiles, 1);
    root = make_dir("root", NULL);
    Directory *cwd = root;

    int choice;
    while (1)
    {
        printf("\nPWD: %s\n", cwd->name);
        printf("1. Create File\n");
        printf("2. Create Directory\n");
        printf("3. Open File\n");
        printf("4. Close File\n");
        printf("5. Search\n");
        printf("6. Print Tree\n");
        printf("7. Change Directory\n");
        printf("8. Exit\n");
        printf("Choice: ");

        if (scanf("%d", &choice) != 1)
            break;
        getchar();

        switch (choice)
        {
        case 1:
            create_file(cwd);
            break;
        case 2:
            create_dir(cwd);
            break;
        case 3:
            open_file();
            break;
        case 4:
            close_file();
            break;
        case 5:
            search_file();
            break;
        case 6:
            print_tree(root, 0);
            break;
        case 7:
        {
            char name[MAX_NAME];
            printf("Enter directory name (.. for parent): ");
            scanf("%s", name);
            if (strcmp(name, "..") == 0)
            {
                if (cwd->parent)
                    cwd = cwd->parent;
            }
            else
            {
                Directory *target = find_subdir(cwd, name);
                if (target)
                    cwd = target;
                else
                    printf("Error: Directory not found.\n");
            }
            break;
        }
        case 8:
            printf("Goodbye.\n");
            return 0;
        default:
            printf("Invalid choice.\n");
        }
    }
    return 0;
}
