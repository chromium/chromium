// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_FILE_SYSTEM_CORE_UTIL_H_
#define COMPONENTS_DRIVE_FILE_SYSTEM_CORE_UTIL_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "components/drive/file_errors.h"
#include "url/gurl.h"

namespace drive {

namespace util {

// "drive" diretory's local ID is fixed to this value.
const char kDriveGrandRootLocalId[] = "<drive>";

// "drive/other" diretory's local ID is fixed to this value.
const char kDriveOtherDirLocalId[] = "<other>";

// "drive/team_drives" diretory's local ID is fixed to this value.
const char kDriveTeamDrivesDirLocalId[] = "<team_drives>";

// "drive/Computers" directory's local ID is fixed to this value.
constexpr char kDriveComputersDirLocalId[] = "<computers>";

// "drive/trash" diretory's local ID is fixed to this value.
const char kDriveTrashDirLocalId[] = "<trash>";

// The directory names used for the Google Drive file system tree. These names
// are used in URLs for the file manager, hence user-visible.
const char kDriveGrandRootDirName[] = "drive";
const char kDriveMyDriveRootDirName[] = "root";
const char kDriveOtherDirName[] = "other";
const char kDriveTeamDrivesDirName[] = "team_drives";
constexpr char kDriveComputersDirName[] = "Computers";
const char kDriveTrashDirName[] = "trash";

// The team_drive_id value that signifies the users default corpus.
constexpr char kTeamDriveIdDefaultCorpus[] = "";

// Returns the path of the top root of the pseudo tree.
const base::FilePath& GetDriveGrandRootPath();

// Returns the path of the directory representing "My Drive".
const base::FilePath& GetDriveMyDriveRootPath();

// Returns the path of the directory representing "Team Drives".
const base::FilePath& GetDriveTeamDrivesRootPath();

// Returns true if |file_path| is a child directory of the team drives root.
bool IsTeamDrivesPath(const base::FilePath& file_path);

// Escapes a file name in Drive cache.
// Replaces percent ('%'), period ('.') and slash ('/') with %XX (hex)
std::string EscapeCacheFileName(const std::string& filename);

// Unescapes a file path in Drive cache.
// This is the inverse of EscapeCacheFileName.
std::string UnescapeCacheFileName(const std::string& filename);

// Converts a numerical changestamp value to a start page token.
std::string ConvertChangestampToStartPageToken(int64_t changestamp);

// Convers a start page token to a numerical changestamp
bool ConvertStartPageTokenToChangestamp(const std::string& stat_page_token,
                                        int64_t* changestamp);

// Converts the given string to a form suitable as a file name. Specifically,
// - Normalizes in Unicode Normalization Form C.
// - Replaces slashes '/' with '_'.
// - Replaces the whole input with "_" if the all input characters are '.'.
// |input| must be a valid UTF-8 encoded string.
std::string NormalizeFileName(const std::string& input);

// Helper to destroy objects which needs Destroy() to be called on destruction.
struct DestroyHelper {
  template <typename T>
  void operator()(T* object) const {
    if (object)
      object->Destroy();
  }
};

// Creates a GDoc file with given values.
//
// GDoc files are used to represent hosted documents on local filesystems.
// A GDoc file contains a JSON whose content is a URL to view the document and
// a resource ID of the entry.
bool CreateGDocFile(const base::FilePath& file_path,
                    const GURL& url,
                    const std::string& resource_id);

// Reads URL from a GDoc file.
GURL ReadUrlFromGDocFile(const base::FilePath& file_path);

// Reads resource ID from a GDoc file.
std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path);

}  // namespace util
}  // namespace drive

#endif  // COMPONENTS_DRIVE_FILE_SYSTEM_CORE_UTIL_H_
