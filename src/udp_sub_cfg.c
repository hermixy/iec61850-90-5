/************************************************************************/
/* SISCO SOFTWARE MODULE HEADER *****************************************/
/************************************************************************/
/*	(c) Copyright Systems Integration Specialists Company, Inc.,	*/
/*	2011-2011, All Rights Reserved					*/
/*									*/
/* MODULE NAME : udp_sub_cfg.c						*/
/* PRODUCT(S)  : MMS-EASE Lite						*/
/*									*/
/* MODULE DESCRIPTION :							*/
/*	Function to read "udp_sub.cfg" input file.			*/
/*									*/
/* GLOBAL FUNCTIONS DEFINED IN THIS MODULE :				*/
/*			udp_sub_cfg_read				*/
/*									*/
/* MODIFICATION LOG :							*/
/*  Date     Who	   Comments					*/
/* --------  ---  ------   -------------------------------------------  */
/* 07/22/11  JRB	   Initial Revision.				*/
/************************************************************************/
//DEBUG: replacing SXLOG macros with something else should avoid
//       unnecessarily linking in sx_dec.c & related parser code.
#include "glbtypes.h"
#include "sysincs.h"
#include "udp.h"
#include "sx_log.h"
#include "str_util.h"	/* for strn..._safe	protos	*/
#ifdef DEBUG_SISCO
SD_CONST static ST_CHAR *SD_CONST thisFileName = __FILE__;
#endif

/************************************************************************/
/*			convert_mac					*/
/* Converts MAC string read from SCL file (like 01-02-03-04-05-06)	*/
/* to 6 byte hex MAC address.						*/
/************************************************************************/
#define MAX_MAC_STRING_LEN	60
static ST_RET convert_mac (ST_UCHAR *dst, ST_CHAR *src)
  {
ST_RET retcode;
ST_CHAR tmpbuf [MAX_MAC_STRING_LEN+1];
ST_CHAR *tmpptr;
ST_UINT dstlen;

  /* Input string may include extra blanks, so allow for fairly long string.*/
  if (strlen (src) > MAX_MAC_STRING_LEN)
    retcode = SD_FAILURE;
  else
    {
    /* Just replace each '-' with ' '. Then use ascii_to_hex_str to convert.*/
    tmpptr = tmpbuf;
    /* Copy until NULL terminator but ignore '-' and spaces.	*/
    for ( ;  *src;  src++)
      {
      if (*src != '-' && (!isspace(*src)))
        *tmpptr++ = *src;
      }
    *tmpptr = '\0';	/* NULL terminate temp buffer	*/
    retcode = ascii_to_hex_str (dst, &dstlen, 6, tmpbuf);
    if (retcode == SD_SUCCESS && dstlen != 6)
      retcode = SD_FAILURE; 
    }
  return (retcode);
  }
    
/************************************************************************/
/*			udp_sub_cfg_read				*/
/* Reads "udp_sub.cfg" input file & fills in UDP_SUB_CFG struct.	*/
/* RETURNS:	SD_SUCCESS or error code				*/
/************************************************************************/
ST_RET udp_sub_cfg_read (
	ST_CHAR *cfg_filename,	/* usually "udp_sub.cfg"	*/
	UDP_SUB_CFG *udp_sub_cfg)
  {
FILE *in_fp;
ST_CHAR in_buf[256];	/* buffer for one line read from file	*/
ST_CHAR token_buf[256];	/* copy of "in_buf". Modified by parsing code.	*/
ST_CHAR *curptr;		/* ptr to current position in token_buf	*/
char seps[] = ",\t\n";
ST_INT line_num;		/* number of lines in file	*/
ST_RET retcode = SD_SUCCESS;
ST_CHAR *parameter_name;		/* first token on line	*/
ST_CHAR *value;			/* 2nd token	*/
ST_CHAR *value2;		/* 3nd token	*/

  memset (udp_sub_cfg, 0, sizeof (UDP_SUB_CFG));	/* CRITICAL: start with clean struct*/
  /* This memset initializes udp_sub_cfg->numIPAddr = 0 &	*/
  /* udp_sub_cfg->numMACAddr = 0				*/

  udp_sub_cfg->IPPort = 102;	/* fixed port. May make it configurable later.*/

  in_fp = fopen (cfg_filename, "r");
  if (in_fp == NULL)
    {
    SXLOG_ERR1 ("Error opening input file '%s'", cfg_filename);
    return (SD_FAILURE);
    }

  /* Read one line at a time from the input file	*/
  line_num = 0;
  while (fgets (in_buf, sizeof(in_buf) - 1, in_fp) != NULL)
    {
    line_num++;

    /* Copy the input buffer to "token_buf". This code modifies the 
     * copied buffer (token_buf). Keep input buffer (in_buf) intact.
     */
    strcpy (token_buf, in_buf);
    curptr = token_buf;	/* init "curptr"	*/
    /* First token must be "ParameterName".	*/
    parameter_name = get_next_string (&curptr, seps);

    /* If NULL, this is empty line. If first char is '#', this is comment line.*/
    if (parameter_name == NULL || parameter_name[0] == '#')
      continue;		/* Ignore empty lines & comment lines	*/
    if (parameter_name [0] == '\0')
      {
      /* First token is empty. This is probably empty line.	*/
      /* Ignore this line, but log error if more tokens found. 	*/
      if ((value = get_next_string (&curptr, seps)) != NULL)
        SXLOG_ERR3 ("Input ignored because first token is empty at line %d in '%s'. Second token='%s'",
                    line_num, cfg_filename, value);
      continue;
      }

    /* Second token must be "Value".	*/
    value          = get_next_string (&curptr, seps);
    value2         = get_next_string (&curptr, seps);	/* need 2nd value for MACAddrRemap	*/

    if (value && value [0] != '\0')
      {
      if (stricmp (parameter_name, "IPAddr") == 0)
        {
        if (udp_sub_cfg->numIPAddr < UDP_MAX_ADDR)
          {
          if (value && value [0] != '\0' && value2 && value2 [0] != '\0')
            {
            strncpy_safe (udp_sub_cfg->IPAddr[udp_sub_cfg->numIPAddr], value, MAX_IDENT_LEN);
            strncpy_safe (udp_sub_cfg->SrcIPAddr[udp_sub_cfg->numIPAddr], value2, MAX_IDENT_LEN);
            udp_sub_cfg->numIPAddr++;
            }
          else
            {
            SXLOG_ERR2 ("Must specify DST and SRC for IPAddr configured at line %d in '%s'.",
                    line_num, cfg_filename);
            retcode = SD_FAILURE;
            }
          }
        else
          {
          SXLOG_ERR2 ("Too many IPAddr configured at line %d in '%s'.",
                    line_num, cfg_filename);
          retcode = SD_FAILURE;
          }
        }
      /* This is used only on UDP Subscriber to remap MAC.*/
      else if (stricmp (parameter_name, "MACAddrRemap") == 0)
        {
        if (udp_sub_cfg->numMACAddr >= UDP_MAX_ADDR)
          {
          SXLOG_ERR0 ("Too many MACAddrRemap entries.");
          retcode = SD_FAILURE;
          }
      	else
          {
          if (convert_mac (udp_sub_cfg->MACin[udp_sub_cfg->numMACAddr],value))	/*assumes format like SCL*/
            {
            SXLOG_ERR1 ("Illegal MAC Address '%s'", value);
            retcode = SD_FAILURE;
            }
          if (convert_mac (udp_sub_cfg->MACout[udp_sub_cfg->numMACAddr],value2))	/*assumes format like SCL*/
            {
            SXLOG_ERR1 ("Illegal MAC Address '%s'", value2);
            retcode = SD_FAILURE;
            }
          udp_sub_cfg->numMACAddr++;
          }
        }
      /* This is used only on UDP Subscriber to remap VLAN_ID.*/
      else if (stricmp (parameter_name, "VLAN_ID") == 0)
        {
        if (asciiToUint16 (value, &udp_sub_cfg->VLAN_ID) != SD_SUCCESS ||
            udp_sub_cfg->VLAN_ID > 4095) 	/* Must fit in 12 bits	*/
          {
          SXLOG_ERR3 ("Illegal VLAN_ID value '%s' at line %d in '%s'.",
                      value, line_num, cfg_filename);
          retcode = SD_FAILURE;
          }
        }
      /* This is used only on UDP Subscriber to remap VLAN_PRIORITY.*/
      else if (stricmp (parameter_name, "VLAN_PRIORITY") == 0)
        {
        if (asciiToUint16 (value, &udp_sub_cfg->VLAN_PRIORITY) != SD_SUCCESS ||
            udp_sub_cfg->VLAN_PRIORITY > 7)	/* Must fit in 3 bits	*/
          {
          SXLOG_ERR3 ("Illegal VLAN_PRIORITY value '%s' at line %d in '%s'.",
                      value, line_num, cfg_filename);
          retcode = SD_FAILURE;
          }
        }
      else
        {
        SXLOG_ERR3 ("Unrecognized ParameterName '%s' at line %d in '%s'.",
                    parameter_name, line_num, cfg_filename);
        retcode = SD_FAILURE;
        }
      }
    else
      {
      SXLOG_ERR2 ("Invalid input at line %d in '%s'. Must contain ParameterName and Value.", line_num, cfg_filename);
      SXLOG_CERR1 ("%s", in_buf);
      retcode = SD_FAILURE;
      }

    /* If ANYTHING failed so far, stop looping.	*/
    if (retcode)
      {
      SXLOG_CERR0 ("Error may be caused by extra delimiter in input treated as empty field");
      break;	/* get out of loop	*/
      }
    }	/* end main "while" loop	*/

  fclose (in_fp);
  return (retcode);
  }
