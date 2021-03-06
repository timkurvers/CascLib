/*****************************************************************************/
/* CascBuildCfg.cpp                       Copyright (c) Ladislav Zezula 2014 */
/*---------------------------------------------------------------------------*/
/* Build configuration for CascLib                                           */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 29.04.14  1.00  Lad  The first version of CascBuildCfg.cpp                */
/*****************************************************************************/

#define __CASCLIB_SELF__
#include "CascLib.h"
#include "CascCommon.h"

//-----------------------------------------------------------------------------
// Local structures

struct TGameIdString
{
    const char * szGameInfo;
    size_t cchGameInfo;
    DWORD dwGameInfo;
};

static TGameIdString GameIds[] =
{
    {"Hero",       0x04, CASC_GAME_HOTS},           // Alpha build of Heroes of the Storm
    {"WoW",        0x03, CASC_GAME_WOW6},           // Alpha build of World of Warcraft - Warlords of Draenor
    {"Diablo3",    0x07, CASC_GAME_DIABLO3},        // Diablo III BETA 2.2.0
    {"Prometheus", 0x0A, CASC_GAME_OVERWATCH},      // Overwatch BETA since build 24919
    {NULL, 0, 0},
};

//-----------------------------------------------------------------------------
// Local functions

static bool inline IsValueSeparator(LPBYTE pbVarValue)
{
    return ((0 <= pbVarValue[0] && pbVarValue[0] <= 0x20) || (pbVarValue[0] == '|'));
}

static bool IsCharDigit(BYTE OneByte)
{
    return ('0' <= OneByte && OneByte <= '9');
}

static void FreeCascBlob(PQUERY_KEY pBlob)
{
    if(pBlob != NULL)
    {
        if(pBlob->pbData != NULL)
            CASC_FREE(pBlob->pbData);

        pBlob->pbData = NULL;
        pBlob->cbData = 0;
    }
}

static DWORD GetLocaleMask(const char * szTag)
{
    if(!strcmp(szTag, "enUS"))
        return CASC_LOCALE_ENUS;

    if(!strcmp(szTag, "koKR"))
        return CASC_LOCALE_KOKR;

    if(!strcmp(szTag, "frFR"))
        return CASC_LOCALE_FRFR;

    if(!strcmp(szTag, "deDE"))
        return CASC_LOCALE_DEDE;

    if(!strcmp(szTag, "zhCN"))
        return CASC_LOCALE_ZHCN;

    if(!strcmp(szTag, "esES"))
        return CASC_LOCALE_ESES;

    if(!strcmp(szTag, "zhTW"))
        return CASC_LOCALE_ZHTW;

    if(!strcmp(szTag, "enGB"))
        return CASC_LOCALE_ENGB;

    if(!strcmp(szTag, "enCN"))
        return CASC_LOCALE_ENCN;

    if(!strcmp(szTag, "enTW"))
        return CASC_LOCALE_ENTW;

    if(!strcmp(szTag, "esMX"))
        return CASC_LOCALE_ESMX;

    if(!strcmp(szTag, "ruRU"))
        return CASC_LOCALE_RURU;

    if(!strcmp(szTag, "ptBR"))
        return CASC_LOCALE_PTBR;

    if(!strcmp(szTag, "itIT"))
        return CASC_LOCALE_ITIT;

    if(!strcmp(szTag, "ptPT"))
        return CASC_LOCALE_PTPT;

    return 0;
}

static bool IsInfoVariable(const char * szLineBegin, const char * szLineEnd, const char * szVarName, const char * szVarType)
{
    size_t nLength;

    // Check the variable name
    nLength = strlen(szVarName);
    if((size_t)(szLineEnd - szLineBegin) > nLength)
    {
        // Check the variable name
        if(!_strnicmp(szLineBegin, szVarName, nLength))
        {
            // Skip variable name and the exclamation mark
            szLineBegin += nLength;
            if(szLineBegin < szLineEnd && szLineBegin[0] == '!')
            {
                // Skip the exclamation mark
                szLineBegin++;

                // Check the variable type
                nLength = strlen(szVarType);
                if((size_t)(szLineEnd - szLineBegin) > nLength)
                {
                    // Check the variable name
                    if(!_strnicmp(szLineBegin, szVarType, nLength))
                    {
                        // Skip variable type and the doublecolon
                        szLineBegin += nLength;
                        return (szLineBegin < szLineEnd && szLineBegin[0] == ':');
                    }
                }
            }
        }
    }

    return false;
}

static const char * SkipInfoVariable(const char * szLineBegin, const char * szLineEnd)
{
    while(szLineBegin < szLineEnd)
    {
        if(szLineBegin[0] == '|')
            return szLineBegin + 1;

        szLineBegin++;
    }

    return NULL;
}

static TCHAR * CheckForIndexDirectory(TCascStorage * hs, const TCHAR * szSubDir)
{
    TCHAR * szIndexPath;

    // Cpmbine the index path
    szIndexPath = CombinePath(hs->szDataPath, szSubDir);
    if(DirectoryExists(szIndexPath))
    {
        hs->szIndexPath = szIndexPath;
        return hs->szIndexPath;
    }

    CASC_FREE(szIndexPath);
    return NULL;
}

TCHAR * AppendBlobText(TCHAR * szBuffer, LPBYTE pbData, DWORD cbData, TCHAR chSeparator)
{
    // Put the separator, if any
    if(chSeparator != 0)
        *szBuffer++ = chSeparator;

    // Copy the blob data as text
    for(DWORD i = 0; i < cbData; i++)
    {
        *szBuffer++ = IntToHexChar[pbData[0] >> 0x04];
        *szBuffer++ = IntToHexChar[pbData[0] & 0x0F];
        pbData++;
    }

    // Terminate the string
    *szBuffer = 0;

    // Return new buffer position
    return szBuffer;
}

static int StringBlobToBinaryBlob(
    PQUERY_KEY pBlob,
    LPBYTE pbBlobBegin, 
    LPBYTE pbBlobEnd)
{
    // Sanity checks
    assert(pBlob != NULL && pBlob->pbData != NULL);

    // Reset the blob length
    pBlob->cbData = 0;

    // Convert the blob
    while(pbBlobBegin < pbBlobEnd)
    {
        BYTE DigitOne;
        BYTE DigitTwo;

        DigitOne = (BYTE)(AsciiToUpperTable_BkSlash[pbBlobBegin[0]] - '0');
        if(DigitOne > 9)
            DigitOne -= 'A' - '9' - 1;

        DigitTwo = (BYTE)(AsciiToUpperTable_BkSlash[pbBlobBegin[1]] - '0');
        if(DigitTwo > 9)
            DigitTwo -= 'A' - '9' - 1;

        if(DigitOne > 0x0F || DigitTwo > 0x0F || pBlob->cbData >= MAX_CASC_KEY_LENGTH)
            return ERROR_BAD_FORMAT;

        pBlob->pbData[pBlob->cbData++] = (DigitOne << 0x04) | DigitTwo;
        pbBlobBegin += 2;
    }

    return ERROR_SUCCESS;
}

static bool GetNextFileLine(PQUERY_KEY pFileBlob, LPBYTE * ppbLineBegin, LPBYTE * ppbLineEnd)
{
    LPBYTE pbLineBegin = *ppbLineBegin;
    LPBYTE pbLineEnd = *ppbLineEnd;
    LPBYTE pbFileEnd = pFileBlob->pbData + pFileBlob->cbData;

    // If there was a previous line, skip all end-of-line chars
    if(pbLineEnd != NULL)
    {
        // Go to the next line
        while(pbLineEnd < pbFileEnd && (pbLineEnd[0] == 0x0A || pbLineEnd[0] == 0x0D))
            pbLineEnd++;
        pbLineBegin = pbLineEnd;

        // If there is no more data, return false
        if(pbLineEnd >= pbFileEnd)
            return false;
    }

    // Skip all spaces before the line begins
    while(pbLineBegin < pbFileEnd && (pbLineBegin[0] == 0x09 || pbLineBegin[0] == 0x20))
        pbLineBegin++;
    pbLineEnd = pbLineBegin;

    // Go to the end of the line
    while(pbLineEnd < pbFileEnd && pbLineEnd[0] != 0x0A && pbLineEnd[0] != 0x0D)
        pbLineEnd++;

    // Give the results to the caller
    *ppbLineBegin = pbLineBegin;
    *ppbLineEnd = pbLineEnd;
    return true;
}

static LPBYTE CheckLineVariable(LPBYTE pbLineBegin, LPBYTE pbLineEnd, const char * szVarName)
{
    size_t nLineLength = (size_t)(pbLineEnd - pbLineBegin);
    size_t nNameLength = strlen(szVarName);

    // If the line longer than the variable name?
    if(nLineLength > nNameLength)
    {
        if(!_strnicmp((const char *)pbLineBegin, szVarName, nNameLength))
        {
            // Skip the variable name
            pbLineBegin += nNameLength;

            // Skip the separator(s)
            while(pbLineBegin < pbLineEnd && IsValueSeparator(pbLineBegin))
                pbLineBegin++;

            // Check if there is "="
            if(pbLineBegin >= pbLineEnd || pbLineBegin[0] != '=')
                return NULL;
            pbLineBegin++;

            // Skip the separator(s)
            while(pbLineBegin < pbLineEnd && IsValueSeparator(pbLineBegin))
                pbLineBegin++;

            // Check if there is "="
            if(pbLineBegin >= pbLineEnd)
                return NULL;

            // Return the begin of the variable
            return pbLineBegin;
        }
    }

    return NULL;
}

static int LoadInfoVariable(PQUERY_KEY pVarBlob, const char * szLineBegin, const char * szLineEnd, bool bHexaValue)
{
    const char * szLinePtr = szLineBegin;

    // Sanity checks
    assert(pVarBlob->pbData == NULL);
    assert(pVarBlob->cbData == 0);

    // Check length of the variable
    while(szLinePtr < szLineEnd && szLinePtr[0] != '|')
        szLinePtr++;

    // Allocate space for the blob
    if(bHexaValue)
    {
        // Initialize the blob
        pVarBlob->pbData = CASC_ALLOC(BYTE, (szLinePtr - szLineBegin) / 2);
        return StringBlobToBinaryBlob(pVarBlob, (LPBYTE)szLineBegin, (LPBYTE)szLinePtr);
    }

    // Initialize the blob
    pVarBlob->pbData = CASC_ALLOC(BYTE, (szLinePtr - szLineBegin) + 1);
    pVarBlob->cbData = (DWORD)(szLinePtr - szLineBegin);

    // Check for success
    if(pVarBlob->pbData == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Copy the string
    memcpy(pVarBlob->pbData, szLineBegin, pVarBlob->cbData);
    pVarBlob->pbData[pVarBlob->cbData] = 0;
    return ERROR_SUCCESS;
}

static char * ExtractStringVariable(PQUERY_KEY pJsonFile, const char * szVarName, size_t * PtrLength)
{
    char * szValueBegin;
    char * szValueStr;
    char * szFileEnd = (char *)(pJsonFile->pbData + pJsonFile->cbData);
    char szQuotedName[0x100];
    int nLength;

    // Find the substring
    nLength = sprintf(szQuotedName, "\"%s\"", szVarName);
    szValueStr = strstr((char *)pJsonFile->pbData, szQuotedName);
    
    // Did we find the variable value?
    if(szValueStr != NULL)
    {
        // Skip the variable name
        szValueStr += nLength;

        // Skip the stuff after
        while(szValueStr < szFileEnd && (szValueStr[0] == ' ' || szValueStr[0] == ':'))
            szValueStr++;

        // The variable value must start with a quotation mark
        if(szValueStr[0] != '"')
            return NULL;
        szValueBegin = ++szValueStr;

        // Find the next quotation mark
        while(szValueStr < szFileEnd && szValueStr[0] != '"')
            szValueStr++;

        // Get the string value
        if(szValueStr > szValueBegin)
        {
            PtrLength[0] = (szValueStr - szValueBegin);
            return szValueBegin;
        }
    }

    return NULL;
}

static void AppendConfigFilePath(TCHAR * szFileName, PQUERY_KEY pFileKey)
{
    size_t nLength = _tcslen(szFileName);

    // If there is no slash, append if
    if(nLength > 0 && szFileName[nLength - 1] != '\\' && szFileName[nLength - 1] != '/')
        szFileName[nLength++] = _T('/');

    // Get to the end of the file name
    szFileName = szFileName + nLength;

    // Append the "config" directory
    _tcscpy(szFileName, _T("config"));
    szFileName += 6;

    // Append the first level directory
    szFileName = AppendBlobText(szFileName, pFileKey->pbData, 1, _T('/'));
    szFileName = AppendBlobText(szFileName, pFileKey->pbData + 1, 1, _T('/'));
    szFileName = AppendBlobText(szFileName, pFileKey->pbData, pFileKey->cbData, _T('/'));
}

static DWORD GetBlobCount(LPBYTE pbLineBegin, LPBYTE pbLineEnd)
{
    DWORD dwBlobCount = 0;

    // Until we find an end of the line
    while(pbLineBegin < pbLineEnd)
    {
        // Skip the blob
        while(pbLineBegin < pbLineEnd && IsValueSeparator(pbLineBegin) == false)
            pbLineBegin++;

        // Increment the number of blobs
        dwBlobCount++;

        // Skip the separator
        while(pbLineBegin < pbLineEnd && IsValueSeparator(pbLineBegin))
            pbLineBegin++;
    }

    return dwBlobCount;
}

static int LoadBlobArray(
    PQUERY_KEY pBlob,
    DWORD dwMaxBlobs,
    LPBYTE pbLineBegin,
    LPBYTE pbLineEnd,
    LPBYTE pbBuffer,
    DWORD dwBufferSize)
{
    LPBYTE pbBlobBegin = pbLineBegin;
    LPBYTE pbBlobEnd = pbLineBegin;
    int nError = ERROR_SUCCESS;

    // Sanity check
    assert(pbBuffer != NULL);

    // Until we find an end of the line
    while(pbBlobBegin < pbLineEnd)
    {
        // Convert the blob from string to binary
        if(dwBufferSize < MAX_CASC_KEY_LENGTH)
            return ERROR_NOT_ENOUGH_MEMORY;

        // Find the end of the text blob
        while(pbBlobEnd < pbLineEnd && IsValueSeparator(pbBlobEnd) == false)
            pbBlobEnd++;

        // Convert the blob from ANSI to binary
        pBlob->pbData = pbBuffer;
        nError = StringBlobToBinaryBlob(pBlob, pbBlobBegin, pbBlobEnd);
        if(nError != ERROR_SUCCESS || dwMaxBlobs == 1)
            break;

        // Move the blob, buffer, and limits
        dwBufferSize -= MAX_CASC_KEY_LENGTH;
        pbBuffer += MAX_CASC_KEY_LENGTH;
        dwMaxBlobs--;
        pBlob++;

        // Skip the separator
        while(pbBlobEnd < pbLineEnd && IsValueSeparator(pbBlobEnd))
            pbBlobEnd++;
        pbBlobBegin = pbBlobEnd;
    }

    return nError;
}

static int LoadSingleBlob(PQUERY_KEY pBlob, LPBYTE pbBlobBegin, LPBYTE pbBlobEnd)
{
    LPBYTE pbBuffer;
    size_t nLength = (pbBlobEnd - pbBlobBegin) / 2;

    // Check maximum size
    if(nLength > MAX_CASC_KEY_LENGTH)
        return ERROR_INVALID_PARAMETER;

    // Allocate the blob buffer
    pbBuffer = CASC_ALLOC(BYTE, MAX_CASC_KEY_LENGTH);
    if(pbBuffer == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    return LoadBlobArray(pBlob, 1, pbBlobBegin, pbBlobEnd, pbBuffer, MAX_CASC_KEY_LENGTH);
}

static PQUERY_KEY LoadMultipleBlobs(LPBYTE pbLineBegin, LPBYTE pbLineEnd, DWORD * pdwBlobCount)
{
    PQUERY_KEY pBlobArray = NULL;
    LPBYTE pbBuffer = NULL;
    DWORD dwBlobCount = GetBlobCount(pbLineBegin, pbLineEnd);
    int nError;

    // Only if there is at least 1 blob
    if(dwBlobCount != 0)
    {
        // Allocate the array of blobs
        pBlobArray = CASC_ALLOC(QUERY_KEY, dwBlobCount);
        if(pBlobArray != NULL)
        {
            // Zero the blob array
            memset(pBlobArray, 0, dwBlobCount * sizeof(QUERY_KEY));

            // Allocate buffer for the blobs
            pbBuffer = CASC_ALLOC(BYTE, dwBlobCount * MAX_CASC_KEY_LENGTH);
            if(pbBuffer != NULL)
            {
                // Zero the buffer
                memset(pbBuffer, 0, dwBlobCount * MAX_CASC_KEY_LENGTH);

                // Load the entire blob array
                nError = LoadBlobArray(pBlobArray, dwBlobCount, pbLineBegin, pbLineEnd, pbBuffer, dwBlobCount * MAX_CASC_KEY_LENGTH);
                if(nError == ERROR_SUCCESS)
                {
                    *pdwBlobCount = dwBlobCount;
                    return pBlobArray;
                }

                // Free the buffer
                CASC_FREE(pbBuffer);
            }

            // Free the array of blobs
            CASC_FREE(pBlobArray);
            pBlobArray = NULL;
        }

        // Reset the blob count
        dwBlobCount = 0;
    }

    *pdwBlobCount = dwBlobCount;
    return pBlobArray;
}

static int LoadTextFile(const TCHAR * szFileName, PQUERY_KEY pFileBlob)
{
    TFileStream * pStream;
    ULONGLONG FileSize = 0;
    int nError = ERROR_SUCCESS;

    // Open the agent file
    pStream = FileStream_OpenFile(szFileName, STREAM_FLAG_READ_ONLY | STREAM_PROVIDER_FLAT | BASE_PROVIDER_FILE);
    if(pStream != NULL)
    {
        // Retrieve its size
        FileStream_GetSize(pStream, &FileSize);

        // Load the file to memory
        if(0 < FileSize && FileSize < 0x100000)
        {
            // Initialize the blob
            pFileBlob->cbData = (DWORD)FileSize;
            pFileBlob->pbData = CASC_ALLOC(BYTE, pFileBlob->cbData + 1);

            // Load the file data into the blob
            if(pFileBlob->pbData != NULL)
            {
                FileStream_Read(pStream, NULL, pFileBlob->pbData, (DWORD)FileSize);
                pFileBlob->pbData[pFileBlob->cbData] = 0;
            }
            else
                nError = ERROR_NOT_ENOUGH_MEMORY;
        }
        else
            nError = ERROR_INVALID_PARAMETER;

        FileStream_Close(pStream);
    }
    else
        nError = GetLastError();

    return nError;
}

static int GetGameType(TCascStorage * hs, LPBYTE pbVarBegin, LPBYTE pbLineEnd)
{
    // Go through all games that we support
    for(size_t i = 0; GameIds[i].szGameInfo != NULL; i++)
    {
        // Check the length of the variable 
        if((size_t)(pbLineEnd - pbVarBegin) == GameIds[i].cchGameInfo)
        {
            // Check the string
            if(!_strnicmp((const char *)pbVarBegin, GameIds[i].szGameInfo, GameIds[i].cchGameInfo))
            {
                hs->dwGameInfo = GameIds[i].dwGameInfo;
                return ERROR_SUCCESS;
            }
        }
    }

    // Unknown/unsupported game
    assert(false);
    return ERROR_BAD_FORMAT;
}

// "B29049"
// "WOW-18125patch6.0.1"
// "30013_Win32_2_2_0_Ptr_ptr"
// "prometheus-0_8_0_0-24919"
static int GetBuildNumber(TCascStorage * hs, LPBYTE pbVarBegin, LPBYTE pbLineEnd)
{
    DWORD dwBuildNumber = 0;

    // Skip all non-digit characters
    while(pbVarBegin < pbLineEnd)
    {
        // There must be at least three digits (build 99 anyone?)
        if(IsCharDigit(pbVarBegin[0]) && IsCharDigit(pbVarBegin[1]) && IsCharDigit(pbVarBegin[2]))
        {
            // Convert the build number string to value
            while(pbVarBegin < pbLineEnd && IsCharDigit(pbVarBegin[0]))
                dwBuildNumber = (dwBuildNumber * 10) + (*pbVarBegin++ - '0');
            break;
        }

        // Move to the next
        pbVarBegin++;
    }

    assert(dwBuildNumber != 0);
    hs->dwBuildNumber = dwBuildNumber;
    return (dwBuildNumber != 0) ? ERROR_SUCCESS : ERROR_BAD_FORMAT;
}

static int GetDefaultLocaleMask(TCascStorage * hs, PQUERY_KEY pTagsString)
{
    char * szTagEnd = (char *)pTagsString->pbData + pTagsString->cbData;
    char * szTagPtr = (char *)pTagsString->pbData;
    char * szNext;
    DWORD dwLocaleMask = 0;

    while(szTagPtr < szTagEnd)
    {
        // Get the next part
        szNext = strchr(szTagPtr, ' ');
        if(szNext != NULL)
            *szNext++ = 0;

        // Check whether the current tag is a language identifier
        dwLocaleMask = dwLocaleMask | GetLocaleMask(szTagPtr);

        // Get the next part
        if(szNext == NULL)
            break;
        
        // Skip spaces
        while(szNext < szTagEnd && szNext[0] == ' ')
            szNext++;
        szTagPtr = szNext;
    }

    hs->dwDefaultLocale = dwLocaleMask;
    return ERROR_SUCCESS;
}

static int FetchAndVerifyConfigFile(TCascStorage * hs, PQUERY_KEY pFileKey, PQUERY_KEY pFileBlob)
{
    TCHAR * szFileName;
    int nError;

    // Construct the local file name
    szFileName = CascNewStr(hs->szDataPath, 8 + 3 + 3 + 32);
    if(szFileName == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Add the part where the config file path is
    AppendConfigFilePath(szFileName, pFileKey);
    
    // Load the config file
    nError = LoadTextFile(szFileName, pFileBlob);
    if(nError == ERROR_SUCCESS)
    {
        // Verify the blob's MD5
        if(!VerifyDataBlockHash(pFileBlob->pbData, pFileBlob->cbData, pFileKey->pbData))
        {
            FreeCascBlob(pFileBlob);
            nError = ERROR_BAD_FORMAT;
        }
    }

    CASC_FREE(szFileName);
    return nError;
}

static int ParseInfoFile(TCascStorage * hs, void * pListFile)
{
    QUERY_KEY Active = {NULL, 0};
    QUERY_KEY TagString = {NULL, 0};
    QUERY_KEY CdnHost = {NULL, 0};
    QUERY_KEY CdnPath = {NULL, 0};
    char szOneLine1[0x200];
    char szOneLine2[0x200];
    size_t nLength1;
    size_t nLength2;
    int nError = ERROR_BAD_FORMAT;

    // Extract the first line, cotaining the headers
    nLength1 = ListFile_GetNextLine(pListFile, szOneLine1, _maxchars(szOneLine1));
    if(nLength1 == 0)
        return ERROR_BAD_FORMAT;

    // Now parse the second and the next lines. We are looking for line
    // with "Active" set to 1
    for(;;)
    {
        const char * szLinePtr1 = szOneLine1;
        const char * szLineEnd1 = szOneLine1 + nLength1;
        const char * szLinePtr2 = szOneLine2;
        const char * szLineEnd2;

        // Read the next line
        nLength2 = ListFile_GetNextLine(pListFile, szOneLine2, _maxchars(szOneLine2));
        if(nLength2 == 0)
            break;
        szLineEnd2 = szLinePtr2 + nLength2;

        // Parse all variables
        while(szLinePtr1 < szLineEnd1)
        {
            // Check for variables we need
            if(IsInfoVariable(szLinePtr1, szLineEnd1, "Active", "DEC"))
                LoadInfoVariable(&Active, szLinePtr2, szLineEnd2, false);
            if(IsInfoVariable(szLinePtr1, szLineEnd1, "Build Key", "HEX"))
                LoadInfoVariable(&hs->CdnBuildKey, szLinePtr2, szLineEnd2, true);
            if(IsInfoVariable(szLinePtr1, szLineEnd1, "CDN Key", "HEX"))
                LoadInfoVariable(&hs->CdnConfigKey, szLinePtr2, szLineEnd2, true);
            if(IsInfoVariable(szLinePtr1, szLineEnd1, "CDN Hosts", "STRING"))
                LoadInfoVariable(&CdnHost, szLinePtr2, szLineEnd2, false);
            if(IsInfoVariable(szLinePtr1, szLineEnd1, "CDN Path", "STRING"))
                LoadInfoVariable(&CdnPath, szLinePtr2, szLineEnd2, false);
            if(IsInfoVariable(szLinePtr1, szLineEnd1, "Tags", "STRING"))
                LoadInfoVariable(&TagString, szLinePtr2, szLineEnd2, false);

            // Move both line pointers
            szLinePtr1 = SkipInfoVariable(szLinePtr1, szLineEnd1);
            if(szLinePtr1 == NULL)
                break;

            szLinePtr2 = SkipInfoVariable(szLinePtr2, szLineEnd2);
            if(szLinePtr2 == NULL)
                break;
        }

        // Stop parsing if found active config
        if(Active.pbData != NULL && *Active.pbData == '1')
            break;

        // Free the blobs
        FreeCascBlob(&CdnHost);
        FreeCascBlob(&CdnPath);
        FreeCascBlob(&TagString);
    }

    // All four must be present
    if(hs->CdnBuildKey.pbData != NULL &&
       hs->CdnConfigKey.pbData != NULL &&
       CdnHost.pbData != NULL &&
       CdnPath.pbData != NULL)
    {
        // Merge the CDN host and CDN path
        hs->szUrlPath = CASC_ALLOC(TCHAR, CdnHost.cbData + CdnPath.cbData + 1);
        if(hs->szUrlPath != NULL)
        {
            CopyString(hs->szUrlPath, (char *)CdnHost.pbData, CdnHost.cbData);
            CopyString(hs->szUrlPath + CdnHost.cbData, (char *)CdnPath.pbData, CdnPath.cbData);
            nError = ERROR_SUCCESS;
        }
    }

    // If we found tags, we can extract language build from it
    if(TagString.pbData != NULL)
        GetDefaultLocaleMask(hs, &TagString);

    FreeCascBlob(&CdnHost);
    FreeCascBlob(&CdnPath);
    FreeCascBlob(&TagString);
    return nError;
}

static int ParseAgentFile(TCascStorage * hs, void * pListFile)
{
    const char * szLinePtr;
    const char * szLineEnd;
    char szOneLine[0x200];
    size_t nLength;
    int nError;

    // Load the single line from the text file
    nLength = ListFile_GetNextLine(pListFile, szOneLine, _maxchars(szOneLine));
    if(nLength == 0)
        return ERROR_BAD_FORMAT;

    // Set the line range
    szLinePtr = szOneLine;
    szLineEnd = szOneLine + nLength;

    // Extract the CDN build key
    nError = LoadInfoVariable(&hs->CdnBuildKey, szLinePtr, szLineEnd, true);
    if(nError == ERROR_SUCCESS)
    {
        // Skip the variable
        szLinePtr = SkipInfoVariable(szLinePtr, szLineEnd);

        // Load the CDN config hash
        nError = LoadInfoVariable(&hs->CdnConfigKey, szLinePtr, szLineEnd, true);
        if(nError == ERROR_SUCCESS)
        {
            // Skip the variable
            szLinePtr = SkipInfoVariable(szLinePtr, szLineEnd);

            // Skip the Locale/OS/code variable
            szLinePtr = SkipInfoVariable(szLinePtr, szLineEnd);

            // Load the URL
            hs->szUrlPath = CascNewStrFromAnsi(szLinePtr, szLineEnd);
            if(hs->szUrlPath == NULL)
                nError = ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    // Verify all variables
    if(hs->CdnBuildKey.pbData == NULL || hs->CdnConfigKey.pbData == NULL || hs->szUrlPath == NULL)
        nError = ERROR_BAD_FORMAT;
    return nError;
}

static int LoadCdnConfigFile(TCascStorage * hs, PQUERY_KEY pFileBlob)
{
    LPBYTE pbLineBegin = pFileBlob->pbData;
    LPBYTE pbVarBegin;
    LPBYTE pbLineEnd = NULL;
    int nError;

    while(pbLineBegin != NULL)
    {
        // Get the next line
        if(!GetNextFileLine(pFileBlob, &pbLineBegin, &pbLineEnd))
            break;

        // Archive group
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "archive-group");
        if(pbVarBegin != NULL)
        {
            nError = LoadSingleBlob(&hs->ArchiveGroup, pbVarBegin, pbLineEnd);
            if(nError != ERROR_SUCCESS)
                return nError;
            continue;
        }

        // Archives
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "archives");
        if(pbVarBegin != NULL)
        {
            hs->pArchiveArray = LoadMultipleBlobs(pbVarBegin, pbLineEnd, &hs->ArchiveCount);
            if(hs->pArchiveArray == NULL || hs->ArchiveCount == 0)
                return ERROR_BAD_FORMAT;
            continue;
        }

        // Patch archive group
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "patch-archive-group");
        if(pbVarBegin != NULL)
        {
            LoadSingleBlob(&hs->PatchArchiveGroup, pbVarBegin, pbLineEnd);
            continue;
        }

        // Patch archives
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "patch-archives");
        if(pbVarBegin != NULL)
        {
            hs->pPatchArchiveArray = LoadMultipleBlobs(pbVarBegin, pbLineEnd, &hs->PatchArchiveCount);
            continue;
        }
    }

    // Check if all required fields are present
    if(hs->ArchiveGroup.pbData == NULL || hs->ArchiveGroup.cbData == 0 || hs->pArchiveArray == NULL || hs->ArchiveCount == 0)
        return ERROR_BAD_FORMAT;

    return ERROR_SUCCESS;
}

static int LoadCdnBuildFile(TCascStorage * hs, PQUERY_KEY pFileBlob)
{
    LPBYTE pbLineBegin = pFileBlob->pbData;
    LPBYTE pbVarBegin;
    LPBYTE pbLineEnd = NULL;

    while(pbLineBegin != NULL)
    {
        // Get the next line
        if(!GetNextFileLine(pFileBlob, &pbLineBegin, &pbLineEnd))
            break;

        // Game name
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "build-product");
        if(pbVarBegin != NULL)
        {
            GetGameType(hs, pbVarBegin, pbLineEnd);
            continue;
        }

        // Game build number
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "build-name");
        if(pbVarBegin != NULL)
        {
            GetBuildNumber(hs, pbVarBegin, pbLineEnd);
            continue;
        }

        // Root
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "root");
        if(pbVarBegin != NULL)
        {
            LoadSingleBlob(&hs->RootKey, pbVarBegin, pbLineEnd);
            continue;
        }

        // Patch
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "patch");
        if(pbVarBegin != NULL)
        {
            LoadSingleBlob(&hs->PatchKey, pbVarBegin, pbLineEnd);
            continue;
        }

        // Download
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "download");
        if(pbVarBegin != NULL)
        {
            LoadSingleBlob(&hs->DownloadKey, pbVarBegin, pbLineEnd);
            continue;
        }

        // Install
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "install");
        if(pbVarBegin != NULL)
        {
            LoadSingleBlob(&hs->InstallKey, pbVarBegin, pbLineEnd);
            continue;
        }

        // Encoding keys
        pbVarBegin = CheckLineVariable(pbLineBegin, pbLineEnd, "encoding");
        if(pbVarBegin != NULL)
        {
            hs->pEncodingKeys = LoadMultipleBlobs(pbVarBegin, pbLineEnd, &hs->EncodingKeys);
            if(hs->pEncodingKeys == NULL || hs->EncodingKeys != 2)
                return ERROR_BAD_FORMAT;

            hs->EncodingKey = hs->pEncodingKeys[0];
            hs->EncodingEKey = hs->pEncodingKeys[1];
            continue;
        }
    }

    // Check the encoding keys
    if(hs->pEncodingKeys == NULL || hs->EncodingKeys == 0)
        return ERROR_BAD_FORMAT;
    return ERROR_SUCCESS;
}

//-----------------------------------------------------------------------------
// Public functions

int LoadBuildInfo(TCascStorage * hs)
{
    QUERY_KEY FileData = {NULL, 0};
    TCHAR * szAgentFile;
    TCHAR * szInfoFile;
    void * pListFile;
    bool bBuildConfigComplete = false;
    int nError = ERROR_SUCCESS;

    // Since HOTS build 30027, the game uses .build.info file for storage info
    if(bBuildConfigComplete == false)
    {
        szInfoFile = CombinePath(hs->szRootPath, _T(".build.info"));
        if(szInfoFile != NULL)
        {
            pListFile = ListFile_OpenExternal(szInfoFile);
            if(pListFile != NULL)
            {
                // Parse the info file
                nError = ParseInfoFile(hs, pListFile);
                if(nError == ERROR_SUCCESS)
                    bBuildConfigComplete = true;
                
                ListFile_Free(pListFile);
            }

            CASC_FREE(szInfoFile);
        }
    }

    // If the info file has not been loaded, try the legacy .build.db
    if(bBuildConfigComplete == false)
    {
        szAgentFile = CombinePath(hs->szRootPath, _T(".build.db"));
        if(szAgentFile != NULL)
        {
            pListFile = ListFile_OpenExternal(szAgentFile);
            if(pListFile != NULL)
            {
                nError = ParseAgentFile(hs, pListFile);
                if(nError == ERROR_SUCCESS)
                    bBuildConfigComplete = true;

                ListFile_Free(pListFile);
            }
            CASC_FREE(szAgentFile);
        }
    }

    // If the .build.info and .build.db file hasn't been loaded, fail it 
    if(nError == ERROR_SUCCESS && bBuildConfigComplete == false)
    {
        nError = ERROR_FILE_CORRUPT;
    }

    // Load the configuration file
    if(nError == ERROR_SUCCESS)
    {
        nError = FetchAndVerifyConfigFile(hs, &hs->CdnConfigKey, &FileData);
        if(nError == ERROR_SUCCESS)
        {
            nError = LoadCdnConfigFile(hs, &FileData);
            FreeCascBlob(&FileData);
        }
    }

    // Load the build file
    if(nError == ERROR_SUCCESS)
    {
        nError = FetchAndVerifyConfigFile(hs, &hs->CdnBuildKey, &FileData);
        if(nError == ERROR_SUCCESS)
        {
            nError = LoadCdnBuildFile(hs, &FileData);
            FreeCascBlob(&FileData);
        }
    }

    // Fill the index directory
    if(nError == ERROR_SUCCESS)
    {
        // First, check for more common "data" subdirectory
        if((hs->szIndexPath = CheckForIndexDirectory(hs, _T("data"))) != NULL)
            return ERROR_SUCCESS;

        // Second, try the "darch" subdirectory (older builds of HOTS - Alpha)
        if((hs->szIndexPath = CheckForIndexDirectory(hs, _T("darch"))) != NULL)
            return ERROR_SUCCESS;

        nError = ERROR_FILE_NOT_FOUND;
    }

    return nError;
}

// Checks whether there is a ".agent.db". If yes, the function
// sets "szRootPath" and "szDataPath" in the storage structure
// and returns ERROR_SUCCESS
int CheckGameDirectory(TCascStorage * hs, TCHAR * szDirectory)
{
    QUERY_KEY AgentFile;
    TCHAR * szFilePath;
    size_t nLength = 0;
    char * szValue;
    int nError = ERROR_FILE_NOT_FOUND;

    // Create the full name of the .agent.db file
    szFilePath = CombinePath(szDirectory, _T(".agent.db"));
    if(szFilePath != NULL)
    {
        // Load the file to memory
        nError = LoadTextFile(szFilePath, &AgentFile);
        if(nError == ERROR_SUCCESS)
        {
            // Extract the data directory from the ".agent.db" file
            szValue = ExtractStringVariable(&AgentFile, "data_dir", &nLength);
            if(szValue != NULL)
            {
                hs->szRootPath = CascNewStr(szDirectory, 0);
                hs->szDataPath = CombinePathAndString(szDirectory, szValue, nLength);
                nError = (hs->szDataPath != NULL) ? ERROR_SUCCESS : ERROR_NOT_ENOUGH_MEMORY;
            }

            // Free the loaded blob
            FreeCascBlob(&AgentFile);
        }

        // Freee the file path
        CASC_FREE(szFilePath);
    }

    return nError;
}
