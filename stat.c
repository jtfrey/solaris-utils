/*
 *  stat.c
 *
 *  Solaris version of the Linux "stat" command.  Retrieves path
 *  metadata for one or more arguments and displays in a nice
 *  readable form:
 *
 *    $ ./stat /
 *      File: `/'
 *      Size: 1024            Blocks: 2          IO Block: 8192 directory
 *    Device: 1540001h/22282241d Inode: 2          Links: 38
 *    Access: (0755/drwxr-xr-x)  Uid: (    0/    root)   Gid: (    0/    root)
 *    Access: 2018-02-01 11:05:32-0500
 *    Modify: 2018-01-23 00:01:38-0500
 *    Change: 2018-01-23 00:01:38-0500
 *
 *
 * Compiles easily with the Sun "cc" compiler:
 *
 *   cc -o stat stat.c
 *
 */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <getopt.h>

//

 static struct option const long_options[] = {
            { "help", no_argument, NULL, 'h' },
            { "dereference", no_argument, NULL, 'L'},
            {NULL, 0, NULL, 0}
          };

//

void
usage(
  const char    *exe
)
{
    printf(
        "usage:\n\n"
        "  %s {options} <path> {<path> ..}\n\n"
        " options:\n\n"
        "  -h/--help            display this help and exit\n"
        "  -L/--dereference     follow symlinks\n"
        "\n"
        ,
        exe
      );
}

//

int
is_device_type(
  mode_t      st_mode
)
{
  switch ( st_mode & S_IFMT ) {
    case S_IFCHR:
    case S_IFBLK:
      return 1;
  }
  return 0;
}

//

int
is_symlink(
  mode_t      st_mode
)
{
  return ( (st_mode & S_IFMT) == S_IFLNK );
}

//

const char*
file_type(
  mode_t      st_mode
)
{
  switch ( st_mode & S_IFMT ) {
    case S_IFIFO:
      return "named fifo";
    case S_IFCHR:
      return "character device";
    case S_IFDIR:
      return "directory";
    case S_IFBLK:
      return "block device";
    case S_IFREG:
      return "regular file";
    case S_IFLNK:
      return "symbolic link";
    case S_IFSOCK:
      return "socket";
    case S_IFDOOR:
      return "door";
    case S_IFPORT:
      return "event port";
  }
  return "<unknown>";
}

//

char
file_type_char(
  mode_t      st_mode
)
{
  switch ( st_mode & S_IFMT ) {
    case S_IFIFO:
      return 'p';
    case S_IFCHR:
      return 'c';
    case S_IFDIR:
      return 'd';
    case S_IFBLK:
      return 'b';
    case S_IFREG:
      return '-';
    case S_IFLNK:
      return 'l';
    case S_IFSOCK:
      return 's';
    case S_IFDOOR:
      return 'D';
    case S_IFPORT:
      return 'P';
  }
  return '?';
}

//

const char*
user_perms(
  mode_t      st_mode
)
{
  static char mode_str[4];

  mode_str[0] = (st_mode & S_IRUSR) ? 'r' : '-';
  mode_str[1] = (st_mode & S_IWUSR) ? 'w' : '-';
  mode_str[2] = (st_mode & S_ISUID) ? ((st_mode & S_IXUSR) ? 's' : 'S') : ((st_mode & S_IXUSR) ? 'x' : '-');
  mode_str[3] = '\0';
  return mode_str;
}

//

const char*
group_perms(
  mode_t      st_mode
)
{
  static char mode_str[4];

  mode_str[0] = (st_mode & S_IRGRP) ? 'r' : '-';
  mode_str[1] = (st_mode & S_IWGRP) ? 'w' : '-';
  mode_str[2] = (st_mode & S_ISGID) ? ((st_mode & S_IXGRP) ? 's' : 'S') : ((st_mode & S_IXGRP) ? 'x' : '-');
  mode_str[3] = '\0';
  return mode_str;
}

//

const char*
other_perms(
  mode_t      st_mode
)
{
  static char mode_str[4];

  mode_str[0] = (st_mode & S_IROTH) ? 'r' : '-';
  mode_str[1] = (st_mode & S_IWOTH) ? 'w' : '-';
  mode_str[2] = (st_mode & S_IXOTH) ? 'x' : '-';
  mode_str[3] = '\0';
  return mode_str;
}

//

const char*
get_uid_string(
  uid_t   the_uid
)
{
  static char     uid_str[1024];
  struct passwd   pinfo;

  if ( getpwuid_r(the_uid, &pinfo, uid_str, sizeof(uid_str)) ) return uid_str;
  return "<unknown>";
}

//

const char*
get_gid_string(
  gid_t   the_gid
)
{
  static char     gid_str[1024];
  struct group    ginfo;

  if ( getgrgid_r(the_gid, &ginfo, gid_str, sizeof(gid_str)) ) return gid_str;
  return "<unknown>";
}

//

const char*
decode_permissions(
  struct stat   *finfo
)
{
  printf("Access: (%04o/%c%s%s%s)  Uid: (%5d/%8s)   Gid: (%5d/%8s)\n",
                  (int)(finfo->st_mode & S_IAMB),
                  file_type_char(finfo->st_mode),
                  user_perms(finfo->st_mode),
                  group_perms(finfo->st_mode),
                  other_perms(finfo->st_mode),
                  (int)finfo->st_uid, get_uid_string(finfo->st_uid),
                  (int)finfo->st_gid, get_gid_string(finfo->st_gid)
                );
}

//

char    atime_str[32];
char    mtime_str[32];
char    ctime_str[32];

void
format_times(
  struct stat   *finfo
)
{
  struct tm     a_time;

  localtime_r(&finfo->st_atime, &a_time);
  strftime(atime_str, sizeof(atime_str), "%Y-%m-%d %H:%M:%S%z", &a_time);
  localtime_r(&finfo->st_mtime, &a_time);
  strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S%z", &a_time);
  localtime_r(&finfo->st_ctime, &a_time);
  strftime(ctime_str, sizeof(ctime_str), "%Y-%m-%d %H:%M:%S%z", &a_time);

}

//

typedef int (*stat_function_ptr)(const char *restrict path, struct stat *restrict buf);

//

int
main(
  int     argc,
  char*   argv[]
)
{
  int     argn = 1;
  int     rc = 0;
  char    opt_c;
  int     follow_symlinks = 0;
  
  while ( (opt_c = getopt_long(argc, argv, "hL", long_options, NULL)) != -1 ) {
    switch ( opt_c ) {
      case 'L':
        follow_symlinks = 1;
        break;
      
      case 'h':
        usage(argv[0]);
        exit(0);
      
    }
  }
  argn = optind;
  if ( argc == argn ) {
    printf("ERROR:  no files provided\n");
    usage(argv[0]);
    rc = EINVAL;
  } else {
    stat_function_ptr   stat_fn = ( follow_symlinks ? stat : lstat );
    
    while ( (rc == 0) && (argn < argc) ) {
      struct stat   finfo;

      if ( stat_fn(argv[argn], &finfo) == 0 ) {
        format_times(&finfo);
        if ( is_symlink(finfo.st_mode) ) {
          char      target[MAXPATHLEN];
          
          if ( readlink(argv[argn], target, sizeof(target)) >= 0 ) {
            printf(
                "  File: `%s' -> `%s'\n",
                argv[argn],
                target
              );
          } else {
            printf(
                "  File: `%s' -> <unable to read target>\n",
                argv[argn]
              );
          }
        } else {
          printf(
              "  File: `%s'\n",
              argv[argn]
            );
        }
        printf(
            "  Size: %-10lld\tBlocks: %-10lld IO Block: %-lld %s\n"
            "Device: %xh/%dd Inode: %-10lld Links: %-lld"
            ,
            (long long int)finfo.st_size, (long long int)finfo.st_blocks, (long long int)finfo.st_blksize, file_type(finfo.st_mode),
            finfo.st_dev, finfo.st_dev, (long long int)finfo.st_ino, (long long int)finfo.st_nlink
          );
        if ( is_device_type(finfo.st_mode) ) {
          printf(" Device type: %d,%d\n", major(finfo.st_rdev), minor(finfo.st_rdev));
        } else {
          printf("\n");
        }
        decode_permissions(&finfo);
        printf(
            "Access: %s\n"
            "Modify: %s\n"
            "Change: %s\n"
            ,
            atime_str,
            mtime_str,
            ctime_str
          );
      } else {
        perror("Unable to stat() file");
        rc = errno;
      }
      argn++;
    }
  }
  return rc;
}

