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

// Converts a numerical changestamp value to a start page token.
std::string ConvertChangestampToStartPageToken(int64_t changestamp);

// Helper to destroy objects which needs Destroy() to be called on destruction.
struct DestroyHelper {
  template <typename T>
  void operator()(T* object) const {
    if (object)
      object->Destroy();
  }
};

// Reads URL from a GDoc file.
GURL ReadUrlFromGDocFile(const base::FilePath& file_path);

// Reads resource ID from a GDoc file.
std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path);

}  // namespace util
}  // namespace drive

#endif  // COMPONENTS_DRIVE_FILE_SYSTEM_CORE_UTIL_H_
