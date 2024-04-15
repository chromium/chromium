// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares helper methods used to schedule files for deletion
// on next reboot.

#ifndef CHROME_INSTALLER_UTIL_DELETE_AFTER_REBOOT_HELPER_H_
#define CHROME_INSTALLER_UTIL_DELETE_AFTER_REBOOT_HELPER_H_

#include <windows.h>

#include <stddef.h>

#include <string>
#include <vector>

namespace base {
class FilePath;
}

// Used by the unit tests.
extern const wchar_t kSessionManagerKey[];
extern const wchar_t kPendingFileRenameOps[];

typedef std::pair<std::wstring, std::wstring> PendingMove;

// Attempts to schedule only the item at path for deletion.
bool ScheduleFileSystemEntityForDeletion(const base::FilePath& path);

// Attempts to recursively schedule the directory for deletion.
bool ScheduleDirectoryForDeletion(const base::FilePath& dir_name);

// Removes all pending moves that are registered for |directory| and all
// elements contained in |directory|.
bool RemoveFromMovesPendingReboot(const base::FilePath& directory);

// Retrieves the list of pending renames from the registry and returns a vector
// containing pairs of strings that represent the operations. If the list
// contains only deletes then every other element will be an empty string
// as per http://msdn.microsoft.com/en-us/library/aa365240(VS.85).aspx.
HRESULT GetPendingMovesValue(std::vector<PendingMove>* pending_moves);

// This returns true if |short_form_needle| is contained in |reg_path| where
// |short_form_needle| is a file system path that has been shortened by
// GetShortPathName and |reg_path| is a path stored in the
// PendingFileRenameOperations key.
bool MatchPendingDeletePath(const base::FilePath& short_form_needle,
                            const base::FilePath& reg_path);

// Converts the strings found in |buffer| to a list of PendingMoves that is
// returned in |value|.
// |buffer| points to a series of pairs of null-terminated wchar_t strings
// followed by a terminating null character.
// |byte_count| is the length of |buffer| in bytes.
// |value| is a pointer to an empty vector of PendingMoves (string pairs).
// On success, this vector contains all of the string pairs extracted from
// |buffer|.
// Returns S_OK on success, E_INVALIDARG if buffer does not meet the above
// specification.
HRESULT MultiSZBytesToStringArray(const char* buffer,
                                  size_t byte_count,
                                  std::vector<PendingMove>* value);

// The inverse of MultiSZBytesToStringArray, this function converts a list
// of string pairs into a byte array format suitable for writing to the
// kPendingFileRenameOps registry value. It concatenates the strings and
// appends an additional terminating null character.
void StringArrayToMultiSZBytes(const std::vector<PendingMove>& strings,
                               std::vector<char>* buffer);

// A helper function for the win32 GetShortPathName that more conveniently
// returns a FilePath. Note that if |path| is not present on the file system
// then GetShortPathName will return |path| unchanged, unlike the win32
// GetShortPathName which will return an error.
base::FilePath GetShortPathName(const base::FilePath& path);

#endif  // CHROME_INSTALLER_UTIL_DELETE_AFTER_REBOOT_HELPER_H_
