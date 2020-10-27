// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace chrome {

bool GetDefaultUserDataDirectory(base::FilePath* result) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    *result = base::FilePath(crosapi::kLacrosUserDataPath);
  } else {
    // For developers on Linux desktop, just pick a reasonable default. Most
    // developers will pass --user-data-dir and override this value anyway.
    *result = base::GetHomeDir().Append(".config").Append("lacros");
  }
  return true;
}

void GetUserCacheDirectory(const base::FilePath& profile_dir,
                           base::FilePath* result) {
  // Chrome OS doesn't allow special cache overrides like desktop Linux.
  *result = profile_dir;
}

bool GetUserDocumentsDirectory(base::FilePath* result) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    *result = base::FilePath(crosapi::kMyFilesPath);
  } else {
    // For developers on Linux desktop, just pick a reasonable default.
    *result = base::GetHomeDir().Append("Documents");
  }
  return true;
}

bool GetUserDownloadsDirectorySafe(base::FilePath* result) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    *result = base::FilePath(crosapi::kDefaultDownloadsPath);
  } else {
    // For developers on Linux desktop, just pick a reasonable default.
    *result = base::GetHomeDir().Append("Downloads");
  }
  return true;
}

bool GetUserDownloadsDirectory(base::FilePath* result) {
  return GetUserDownloadsDirectorySafe(result);
}

bool GetUserMusicDirectory(base::FilePath* result) {
  // Chrome OS does not support custom media directories.
  return false;
}

bool GetUserPicturesDirectory(base::FilePath* result) {
  // Chrome OS does not support custom media directories.
  return false;
}

bool GetUserVideosDirectory(base::FilePath* result) {
  // Chrome OS does not support custom media directories.
  return false;
}

bool ProcessNeedsProfileDir(const std::string& process_type) {
  // We have no reason to forbid this on Chrome OS as we don't have roaming
  // profile troubles there.
  return true;
}

}  // namespace chrome
