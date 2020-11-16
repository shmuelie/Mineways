#include <windows.h>
#include <assert.h>
#include <tchar.h>
#include <stdio.h>
#include "tiles.h"
#include "tilegrid.h"

void initializeFileGrid(FileGrid* pfg)
{
	int i;
	pfg->fileCount = 0;
	pfg->totalCategories = TOTAL_CATEGORIES;
	pfg->totalTiles = TOTAL_TILES;
	for (i = 0; i < TOTAL_CATEGORIES; i++) {
		pfg->categories[i] = 0;
	}
	for (i = 0; i < TOTAL_CATEGORIES * TOTAL_TILES; i++) {
		pfg->fr[i].rootName = NULL;
		pfg->fr[i].fullFilename = NULL;
		pfg->fr[i].path = NULL;
		pfg->fr[i].exists = false;
	}
}

void initializeChestGrid(ChestGrid* pcg)
{
	int i;
	pcg->chestCount = 0;
	pcg->totalCategories = TOTAL_CATEGORIES;
	pcg->totalTiles = TOTAL_CHEST_TILES;
	for (i = 0; i < TOTAL_CATEGORIES; i++) {
		pcg->categories[i] = 0;
	}
	for (i = 0; i < TOTAL_CATEGORIES * TOTAL_CHEST_TILES; i++) {
		pcg->cr[i].rootName = NULL;
		pcg->cr[i].fullFilename = NULL;
		pcg->cr[i].path = NULL;
		pcg->cr[i].exists = false;
	}
}

void addBackslashIfNeeded(wchar_t* dir)
{
	if (wcslen(dir) > 0 && 
	    ((wcscmp(&dir[wcslen(dir) - 1], L"\\") != 0) && (wcscmp(&dir[wcslen(dir) - 1], L"/") != 0)))
	{
		wcscat_s(dir, MAX_PATH, L"\\");
	}
}

// return negative number for error type
int searchDirectoryForTiles(FileGrid* pfg, ChestGrid* pcg, const wchar_t* tilePath, int verbose, int alternate, boolean topmost)
{
	int filesProcessed = 0;
	int filesSubProcessed = 0;
	HANDLE hFind;
	WIN32_FIND_DATA ffd;

	wchar_t tilePathAppended[MAX_PATH_AND_FILE];
	wcscpy_s(tilePathAppended, MAX_PATH_AND_FILE, tilePath);
	addBackslashIfNeeded(tilePathAppended);

	wchar_t tileSearch[MAX_PATH_AND_FILE];
	wcscpy_s(tileSearch, MAX_PATH_AND_FILE, tilePathAppended);
	wcscat_s(tileSearch, MAX_PATH_AND_FILE, L"*");
	hFind = FindFirstFile(tileSearch, &ffd);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		//wsprintf(gErrorString, L"***** ERROR: cannot find files for the directory '%s' (Windows error code # %d). Ignoring directory.\n", tilePath, GetLastError());
		//saveErrorForEnd();
		//gErrorCount++;
		return -1;
	}
	else {
		boolean chestFound = false;
		do {
			// is it a directory?
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// if it's just a relative "same" or "above" directory, ignore
				if ((!lstrcmpW(ffd.cFileName, L".")) || (!lstrcmpW(ffd.cFileName, L".."))) {
					continue;
				}

				wchar_t subdir[MAX_PATH_AND_FILE];
				wcscpy_s(subdir, MAX_PATH_AND_FILE, tilePathAppended);
				wcscat_s(subdir, MAX_PATH_AND_FILE, ffd.cFileName);
				addBackslashIfNeeded(subdir);

				int fileCount = searchDirectoryForTiles(pfg, pcg, subdir, verbose, alternate, false);
				if (fileCount < 0) {
					// error, cannot read subdirectory for some reason - we just ignore it for now; main test is the top directory test, above.
					//return -2;
				}
				else {
					filesSubProcessed += fileCount;
				}
			}
			else {
				// not a directory; is it a PNG file?
				int used = FILE_NOT_FOUND;
				// check for blocks only if word "block" is in the path
				if (topmost || wcsstr(tilePathAppended, L"block") != NULL) {
					used = testIfTileExists(pfg, tilePathAppended, ffd.cFileName, verbose, alternate, false) ? 1 : 0;
				}
				if (used == FILE_NOT_FOUND) {
					if (topmost || wcsstr(tilePathAppended, L"chest") != NULL) {
						used = testIfChestFile(pcg, tilePathAppended, ffd.cFileName, verbose);
						chestFound = true;
					}
				}

				if (used == FILE_FOUND) {
					filesProcessed++;
				}
				// squirrelly: have we already found some useful PNG in this directory, and is this not a chest directory?
				// 
				else if (filesProcessed > 0 && !chestFound && isPNGfile(ffd.cFileName)) {
					// we already found some good files in this directory, so note that this file was not used.
					if (verbose) {
						wprintf(L"WARNING: The file '%s' in directory '%s' is not recognized and so is not used.\n", ffd.cFileName, tilePath);
					}
					else {
						wprintf(L"WARNING: The file '%s' is not recognized and so is not used.\n", ffd.cFileName);
					}
				}
			}
		} while (FindNextFile(hFind, &ffd) != 0);

		FindClose(hFind);
	}
	return filesProcessed + filesSubProcessed;
}

// from https://stackoverflow.com/questions/8233842/how-to-check-if-directory-exist-using-c-and-winapi
bool dirExists(const wchar_t* path)
{
	DWORD ftyp = GetFileAttributesW(path);
	if (ftyp == INVALID_FILE_ATTRIBUTES)
		return false;  //something is wrong with your path!

	if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
		return true;   // this is a directory!

	return false;    // this is not a directory!
}

bool createDir(const wchar_t* path)
{
	if (CreateDirectory(path, NULL)) {
		return true;
	}
	else {
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS) {
			return true;
		}
	}
	return false;
}

// returns number of useful tiles found
int checkTilesInDirectory(FileGrid* pfg, const wchar_t* tilePath, int verbose, int alternate)
{
	HANDLE hFind;
	WIN32_FIND_DATA ffd;
	int filesFound = 0;

	wchar_t tileSearch[MAX_PATH];
	wcscpy_s(tileSearch, MAX_PATH, tilePath);
	wcscat_s(tileSearch, MAX_PATH, L"*.png");
	hFind = FindFirstFile(tileSearch, &ffd);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		// go through all the files in the blocks directory
		do {
			filesFound += (testIfTileExists( pfg, tilePath, ffd.cFileName, verbose, alternate, true) == FILE_FOUND) ? 1 : 0;
		} while (FindNextFile(hFind, &ffd) != 0);

		FindClose(hFind);
	}
	return filesFound;
}

// returns 1 if file exists and is usable (not a duplicate, alternate name of something already in use), 2 if found and known to be ignorable
int testIfTileExists(FileGrid* pfg, const wchar_t* tilePath, const wchar_t* origTileName, int verbose, int alternate, boolean warnUnused)
{
	wchar_t tileName[MAX_PATH];

	wcscpy_s(tileName, MAX_PATH, origTileName);

	if (removePNGsuffix(tileName)) {
		if (verbose) {
			wprintf(L"The file '%s' has been found and will be tested to see if it's needed.\n", origTileName);
		}

		// has a PNG suffix, now removed, so test if it's a file name type we understand.
		int category = stripTypeSuffix(tileName, gCatSuffixes, TOTAL_CATEGORIES);
		assert(category >= 0);
		// return a negative value from findTileIndex if tile is not found in any way
		int index = findTileIndex(tileName, alternate);
		if (index >= 0) {
			int fullIndex = category * pfg->totalTiles + index;
			if (pfg->fr[fullIndex].exists) {
				// duplicate, so warn and exit
				if (wcscmp(origTileName, pfg->fr[fullIndex].fullFilename) == 0) {
					wprintf(L"WARNING: duplicate file ignored. File '%s' in directory '%s' is in a different location for the same texture '%s' in '%s'.\n", origTileName, tilePath, pfg->fr[fullIndex].fullFilename, pfg->fr[fullIndex].path);
				}
				else if (verbose) {
					wprintf(L"WARNING: duplicate file ignored. File '%s' in directory '%s' is a different name for the same texture '%s' in '%s'.\n", origTileName, tilePath, pfg->fr[fullIndex].fullFilename, pfg->fr[fullIndex].path);
				}
				else {
					wprintf(L"WARNING: duplicate file ignored. File '%s' is a different name for the same texture '%s'.\n", origTileName, pfg->fr[fullIndex].fullFilename);
				}
				return FILE_FOUND_AND_IGNORED;
			}
			else {
				// it's new and unique
				pfg->fileCount++;
				pfg->categories[category]++;
				pfg->fr[fullIndex].rootName = _wcsdup(tileName);
				pfg->fr[fullIndex].fullFilename = _wcsdup(origTileName);
				pfg->fr[fullIndex].path = _wcsdup(tilePath);
				pfg->fr[fullIndex].exists = true;
				return FILE_FOUND;
			}
		}
		// check if on "unused" list - if so, then we don't issue a warning but just return
		int i = 0;
		while (wcslen(gUnneeded[i]) > 0)
		{
			if (_wcsicmp(tileName, gUnneeded[i]) == 0) {
				return FILE_FOUND_AND_IGNORED;
			}
			i++;
		}
	}

	// unknown file name
	if (warnUnused) {
		wprintf(L"WARNING: The file '%s' is not recognized and so is not used.\n", origTileName);
	}
	return FILE_NOT_FOUND;
}

// returns true if file exists and is usable (not a duplicate, alternate name of something already in use)
int testIfChestFile(ChestGrid* pcg, const wchar_t* tilePath, const wchar_t* origTileName, int verbose)
{
	wchar_t tileName[MAX_PATH_AND_FILE];

	wcscpy_s(tileName, MAX_PATH_AND_FILE, origTileName);

	if (removePNGsuffix(tileName)) {
		// has a PNG suffix, now removed, so test if it's a file name type we understand.
		int type = stripTypeSuffix(tileName, gCatSuffixes, TOTAL_CATEGORIES);

		// the four PNG files we care about
		boolean found = false;
		int index = 0;
		for (int i = 0; i < TOTAL_CHEST_TILES && !found; i++) {
			if (_wcsicmp(tileName, gChestNames[i]) == 0) {
				index = i;
				found = true;
			}
		}

		if (found) {
			int fullIndex = type * pcg->totalTiles + index;
			if (pcg->cr[fullIndex].exists) {
				// duplicate, so warn and exit
				if (verbose) {
					wprintf(L"WARNING: duplicate file ignored. File '%s' in directory '%s' is a different name for the same texture '%s' in '%s'.\n", origTileName, tilePath, pcg->cr[fullIndex].fullFilename, pcg->cr[fullIndex].path);
				}
				else {
					wprintf(L"WARNING: duplicate file ignored. File '%s' is a different name for the same texture '%s'.\n", origTileName, pcg->cr[fullIndex].fullFilename);
				}
				return FILE_NOT_FOUND;
			}
			else {
				// it's new and unique
				pcg->chestCount++;
				pcg->categories[type]++;
				pcg->cr[fullIndex].rootName = _wcsdup(tileName);
				pcg->cr[fullIndex].fullFilename = _wcsdup(origTileName);
				pcg->cr[fullIndex].path = _wcsdup(tilePath);
				pcg->cr[fullIndex].exists = true;
				return FILE_FOUND;
			}
		}
	}
	return FILE_NOT_FOUND;
}

boolean removePNGsuffix(wchar_t* name)
{
	// check for .png suffix - note test is case insensitive
	int len = (int)wcslen(name);
	if (len > 4 && _wcsicmp(&name[len - 4], L".png") == 0)
	{
		// remove .png suffix
		name[len - 4] = 0x0;
		return true;
	}
	return false;
}

boolean isPNGfile(wchar_t* name)
{
	// check for .png suffix - note test is case insensitive
	int len = (int)wcslen(name);
	if (len > 4 && _wcsicmp(&name[len - 4], L".png") == 0)
	{
		return true;
	}
	return false;
}

// return -1 if no suffix matches
int stripTypeSuffix(wchar_t* tileName, const wchar_t** suffixes, int numSuffixes)
{
	int type = 0;
	// ignore first suffix, which is "", which anything will match.
	for (int i = 1; i < numSuffixes; i++) {
		int suffixLen = (int)wcslen(suffixes[i]);
		if (suffixLen > 0) {
			int len = (int)wcslen(tileName);
			if (_wcsicmp(&tileName[len - suffixLen], suffixes[i]) == 0) {
				// now for the annoying exceptions:
				//  L"piston_top_normal",
				//	L"rail_normal",
				//	L"sandstone_normal",
				//	L"red_sandstone_normal",
				// We test if the "non-stripped" name with the suffix actually
				// already matches a given name. If so, don't strip the suffix.
				bool stripSuffix = true;
				if (wcscmp(suffixes[i], gCatSuffixes[CATEGORY_NORMALS_LONG]) == 0) {
					if (findTileIndex(tileName, 1)>=0) {
						stripSuffix = false;
					}
				}
				if (stripSuffix) {
					tileName[len - suffixLen] = 0x0;
					type = i;
					break;
				}
			}
		}
	}
	return type;
}

int findTileIndex(const wchar_t* tileName, int alternate)
{
	int i;
	int index = -1;

	for (i = 0; i < TOTAL_TILES; i++)
	{
		if (_wcsicmp(tileName, gTilesTable[i].filename) == 0) {
			return i;
		}
		if (alternate && _wcsicmp(tileName, gTilesTable[i].altFilename) == 0) {
			return i;
		}
	}

	// none of those worked, so now try some more rules - good times!
	if ( alternate > 1) {
		i = 0;
		while (wcslen(gTilesAlternates[i].filename) > 0) {
			if (_wcsicmp(tileName, gTilesAlternates[i].altFilename) == 0) {
				// tricksy - search only the normal names to find the index of this alternate name
				return findTileIndex(gTilesAlternates[i].filename, 1);
			}
			i++;
		}
	}

	return index;
}

void copyFileRecord(FileGrid *pfg, int category, int fullIndex, FileRecord* srcFR)
{
	pfg->fileCount++;
	pfg->categories[category]++;
	pfg->fr[fullIndex].rootName = _wcsdup(srcFR->rootName);
	pfg->fr[fullIndex].fullFilename = _wcsdup(srcFR->fullFilename);
	pfg->fr[fullIndex].path = _wcsdup(srcFR->path);
	pfg->fr[fullIndex].exists = true;
}

void deleteFileFromGrid(FileGrid* pfg, int category, int fullIndex)
{
	// shouldn't be calling this otherwise, but let's be safe:
	if (pfg->fr[fullIndex].exists) {
		pfg->fileCount--;
		pfg->categories[category]--;
		pfg->fr[fullIndex].exists = false;
	}
	else {
		assert(0);
	}
}

void deleteChestFromGrid(ChestGrid* pcg, int category, int fullIndex)
{
	// shouldn't be calling this otherwise, but let's be safe:
	if (pcg->cr[fullIndex].exists) {
		pcg->chestCount--;
		pcg->categories[category]--;
		pcg->cr[fullIndex].exists = false;
	}
	else {
		assert(0);
	}
}