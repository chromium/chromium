// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_FILE_SYSTEM_CORE_UTIL_H_
#define COMPONENTS_DRIVE_FILE_SYSTEM_CORE_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "components/drive/drive_export.h"
#include "components/drive/file_errors.h"
#include "url/gurl.h"

namespace drive {

namespace util {

// "drive" diretory's local ID is fixed to this value.
inline constexpr char kDriveGrandRootLocalId[] = "<drive>";

// "drive/other" diretory's local ID is fixed to this value.
inline constexpr char kDriveOtherDirLocalId[] = "<other>";

// "drive/team_drives" diretory's local ID is fixed to this value.
inline constexpr char kDriveTeamDrivesDirLocalId[] = "<team_drives>";

// "drive/Computers" directory's local ID is fixed to this value.
inline constexpr char kDriveComputersDirLocalId[] = "<computers>";

// "drive/trash" diretory's local ID is fixed to this value.
inline constexpr char kDriveTrashDirLocalId[] = "<trash>";

// The directory names used for the Google Drive file system tree. These names
// are used in URLs for the file manager, hence user-visible.
inline constexpr char kDriveGrandRootDirName[] = "drive";
inline constexpr char kDriveMyDriveRootDirName[] = "root";
inline constexpr char kDriveOtherDirName[] = "other";
inline constexpr char kDriveTeamDrivesDirName[] = "team_drives";
inline constexpr char kDriveComputersDirName[] = "Computers";
inline constexpr char kDriveTrashDirName[] = "trash";

// The team_drive_id value that signifies the users default corpus.
inline constexpr char kTeamDriveIdDefaultCorpus[] = "";

// Returns the path of the top root of the pseudo tree.
COMPONENTS_DRIVE_EXPORT
const base::FilePath& GetDriveGrandRootPath();

// Converts a numerical changestamp value to a start page token.
COMPONENTS_DRIVE_EXPORT
std::string ConvertChangestampToStartPageToken(int64_t changestamp);

// Helper to destroy objects which needs Destroy() to be called on destruction.
struct COMPONENTS_DRIVE_EXPORT DestroyHelper {
  template <typename T>
  void operator()(T* object) const {
    if (object)
      object->Destroy();
  }
};

// Reads URL from a GDoc file.
COMPONENTS_DRIVE_EXPORT
GURL ReadUrlFromGDocFile(const base::FilePath& file_path);

// Reads resource ID from a GDoc file.
COMPONENTS_DRIVE_EXPORT
std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path);

}  // namespace util
}  // namespace drive

#endif  // COMPONENTS_DRIVE_FILE_SYSTEM_CORE_UTIL_H_
