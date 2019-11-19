// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_FILE_PATH_SANITIZATION_H_
#define CHROME_CHROME_CLEANER_OS_FILE_PATH_SANITIZATION_H_

#include <windows.h>

#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"

namespace chrome_cleaner {

namespace sanitization_internal {

// This is only exported so it can be used by tests. It is terminated by an
// entry where path==nullptr.
//
// CSIDL values for paths which are replaced and the strings they are replaced
// with in the order the replacement is performed in SanitizePath.
extern const int PATH_CSIDL_START;
extern const int PATH_CSIDL_END;
struct rewrite_rule_kvpair {
  int id;
  const wchar_t* path;
};
extern const rewrite_rule_kvpair rewrite_rules[];

}  // namespace sanitization_internal

// Initializes static variables and state required for this library to function
// properly.
void InitializeFilePathSanitization();

// Returns the paths that are subject to path sanitization. Paths returned are
// normalized.
std::vector<base::FilePath> GetRewrittenPaths();

// Returns the map of key paths to the corresponding sanitization string.
std::map<int, base::string16> PathKeyToSanitizeString();

// Convert a CSIDL to a key that can be used with PathService::Get().
int CsidlToPathServiceKey(int CSIDL);

// Return the long path equivalent of |path| in |long_path|.
void ConvertToLongPath(const base::string16& path, base::string16* long_path);

// Converts a FilePath to a common format to prevent comparison errors because
// of case sensitivity or short vs. long path formats.
base::FilePath NormalizePath(const base::FilePath& path);

// Return the value of the |path| after being sanitized. A path is sanitized by
// replacing the portion that represents a CSIDL. Must never be called before
// InitializeFilePathSanitization().
base::string16 SanitizePath(const base::FilePath& path);

// Return the command line string after the executable path is sanitized.
base::string16 SanitizeCommandLine(const base::CommandLine& command_line);

// If |input_path| is a relative path, |csidl| is used as one of the CSIDL_*
// #defines in shlobj.h to identify the root path, to which |input_path is
// appended. If |input_path| is an absolute path, then |csidl| is ignored and
// this becomes a NOOP. If there would be an known invalid CSIDL_ value that
// could be used to identify a NOOP, then we could simply DCHECK that the path
// is absolute in that case. The returned FilePath contains the potentially
// expanded full absolute path.
base::FilePath ExpandSpecialFolderPath(int csidl,
                                       const base::FilePath& input_path);

// Returns true iff |file_path| is safe to access from the sandbox.
bool ValidateSandboxFilePath(const base::FilePath& file_path);

// Returns true iff |file_attributes| contains no attributes that indicate a
// remote file, such as FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS.
bool IsLocalFileAttributes(DWORD file_attributes);

// Returns true iff |file_path|'s attributes can be retrieved and they indicate
// a local file according to IsLocalFileAttributes.
bool IsFilePresentLocally(const base::FilePath& file_path);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_FILE_PATH_SANITIZATION_H_
