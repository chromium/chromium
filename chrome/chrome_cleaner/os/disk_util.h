// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_DISK_UTIL_H_
#define CHROME_CHROME_CLEANER_OS_DISK_UTIL_H_

#include <windows.h>

#include <shlobj.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/os/disk_util_types.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace base {
class FilePath;
}

namespace chrome_cleaner {

class LayeredServiceProviderAPI;

typedef base::OnceCallback<bool(const base::FilePath&)>
    IgnoredReportingCallback;

// Return the full path of the relative path |input_path| when expanded to the
// 64 bits program files path. Return an empty path when not running on 64 bits
// OS.
base::FilePath GetX64ProgramFilesPath(const base::FilePath& input_path);

// Return the full path of the relative path |input_path| when expanded to the
// 32 bits program files path.
base::FilePath GetX86ProgramFilesPath(const base::FilePath& input_path);

// Enumerate matching paths for a |root_path| that contains wild-cards.
// Wild-cards are allowed in any path component other than the first. Matching
// paths are appended to |matches| in a cumulative way.
void CollectMatchingPaths(const base::FilePath& root_path,
                          std::vector<base::FilePath>* matches);

// Return true when a file path contains the wild-card characters '*' or '?'.
bool PathContainsWildcards(const base::FilePath& file_path);

// Initializes static variables and state required for this library to function
// properly.
void InitializeDiskUtil();

// To properly compare wchar_t*. This avoids using C++ string which would
// cause unnecessary allocations, string copies, and static cleanup on
// shutdown.
struct ExtensionsCompare {
  bool operator()(const wchar_t* smaller, const wchar_t* larger) const {
    return ::_wcsicmp(smaller, larger) < 0;
  }
};
typedef std::set<const wchar_t*, ExtensionsCompare> ExtensionSet;

// Return true if |path| has a active file extension.
bool PathHasActiveExtension(const base::FilePath& file_path);

// Expand environment variables in path into expanded_path. When called
// expanded_path must be an empty path. If any component of path contains
// environment variables expands to more than MAX_PATH characters the function
// will fail and return false.
bool ExpandEnvPath(const base::FilePath& path, base::FilePath* expanded_path);

// Replace an absolute file path by its WOW64 folder equivalent.
void ExpandWow64Path(const base::FilePath& path, base::FilePath* expanded_path);

// Return a string16 representation of |file_information|.
base::string16 FileInformationToString(
    const internal::FileInformation& file_information);

// Returns true if the given |company_name| is on the list of companies whose
// executables' details should not be reported.
bool IsCompanyOnIgnoredReportingList(const base::string16& company_name);

// Returns true if the given |path| refers to an executable whose details
// should not be reported.
bool IsExecutableOnIgnoredReportingList(const base::FilePath& file_path);

// Retrieve the detailed information for the executable |file_path| and append
// the fields to |file_information|. If the executable sets |ignored_reporting|
// according to the given |ignored_reporting_callback|, |file_information| stays
// unchanged.
bool RetrieveDetailedFileInformation(
    const base::FilePath& file_path,
    internal::FileInformation* file_information,
    bool* ignored_reporting,
    IgnoredReportingCallback ignored_reporting_callback =
        base::BindOnce(&IsExecutableOnIgnoredReportingList));

// Retrieve the file information path, dates and size into |file_information|.
bool RetrieveBasicFileInformation(const base::FilePath& file_path,
                                  internal::FileInformation* file_information);

// If |include_details|, retrieves detailed file information for |file_path|.
// Otherwise, only basic information is retrieved.
bool RetrieveFileInformation(const base::FilePath& file_path,
                             bool include_details,
                             internal::FileInformation* file_information);

// Compute the SHA256 checksum of |path| and store it as base16 into |digest|.
// Return true on success.
bool ComputeSHA256DigestOfPath(const base::FilePath& path, std::string* digest);

// Compute the SHA256 of |content| and store it as base16 into |digest|.
// Return true on success.
bool ComputeSHA256DigestOfString(const std::string& content,
                                 std::string* digest);

// Return the list of registered Layered Service Providers. In case the same DLL
// is registered with multiple ProviderId, |providers| is a map from the DLL
// FilePath to a list of ProviderIds.
struct GUIDLess {
  bool operator()(const GUID& smaller, const GUID& larger) const;
};

struct FilePathLess {
  bool operator()(const base::FilePath& smaller,
                  const base::FilePath& larger) const;
};

typedef std::map<base::FilePath, std::set<GUID, GUIDLess>, FilePathLess>
    LSPPathToGUIDs;
void GetLayeredServiceProviders(const LayeredServiceProviderAPI& lsp_api,
                                LSPPathToGUIDs* providers);

// Delete the file |path| from another process after getting it to sleep for
// |delay_before_delete_ms| and return the handle of that other process in
// |process_handle|, which can be null if the handle is not needed.
bool DeleteFileFromTempProcess(const base::FilePath& path,
                               uint32_t delay_before_delete_ms,
                               base::win::ScopedHandle* process_handle);

// Return true if both paths represent the same file. This function takes care
// of short/long path and case sensitive path.
bool PathEqual(const base::FilePath& path1, const base::FilePath& path2);

// Retrieve and create if needed the folder to the Chrome Cleanup folder under
// local app data. Return true on success.
bool GetAppDataProductDirectory(base::FilePath* path);

// Retrieve existing 32-bit and 64-bit program files folders.
void GetProgramFilesFolders(std::set<base::FilePath>* folders);

// Retrieve existing 32-bit and 64-bit program files\common files folders.
void GetProgramFilesCommonFolders(std::set<base::FilePath>* folders);

// Retrieve every root folder where program can be installed. This is useful
// because some programs do not install at the same place without administrator
// rights.
void GetAllProgramFolders(std::set<base::FilePath>* folders);

// Return true if |path| has a Zone Identifier.
bool HasZoneIdentifier(const base::FilePath& path);

// Overwrite the Zone Identifier to "Local Machine". Return true on success.
bool OverwriteZoneIdentifier(const base::FilePath& path);

// Return a file path found in the content text, typically from a registry entry
// that may have arguments, or be the argument of a rundll.
base::FilePath ExtractExecutablePathFromRegistryContent(
    const base::string16& content);

// Perform environment variable expansion and wow64 path replacement.
base::FilePath ExpandEnvPathAndWow64Path(const base::FilePath& path);

// Return true if |name| contains wildcard characters '?' or '*'.
bool NameContainsWildcards(const base::string16& name);

// See |String16WildcardMatchInsensitive|.
bool NameMatchesPattern(const base::string16& name,
                        const base::string16& pattern,
                        const wchar_t escape_char);

// Retrieves basic path information for |expanded_path|.
void RetrievePathInformation(const base::FilePath& expanded_path,
                             internal::FileInformation* file_information);

// Tries to expand |file_path|, copying the result to |expanded_path|. Returns
// true if successful.
bool TryToExpandPath(const base::FilePath& file_path,
                     base::FilePath* expanded_path);

// If the file is larger than |tail_size_bytes|, it will get truncated to the
// first newline character within last |tail_size_bytes|.
void TruncateLogFileToTail(const base::FilePath& path, int64_t tail_size_bytes);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_DISK_UTIL_H_
