// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_paths_lacros.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace chrome {
namespace {

struct DefaultPaths {
  base::FilePath documents_dir;
  base::FilePath downloads_dir;
};

DefaultPaths& GetDefaultPaths() {
  static base::NoDestructor<DefaultPaths> lacros_paths;
  return *lacros_paths;
}

}  // namespace

void SetLacrosDefaultPaths(const base::FilePath& documents_dir,
                           const base::FilePath& downloads_dir) {
  DCHECK(!documents_dir.empty());
  DCHECK(documents_dir.IsAbsolute());
  GetDefaultPaths().documents_dir = documents_dir;

  DCHECK(!downloads_dir.empty());
  DCHECK(downloads_dir.IsAbsolute());
  GetDefaultPaths().downloads_dir = downloads_dir;
}

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
  // NOTE: Lacros overrides the path with a value from ash early in startup. See
  // crosapi::mojom::LacrosInitParams.
  CHECK(!GetDefaultPaths().documents_dir.empty());
  *result = GetDefaultPaths().documents_dir;
  return true;
}

bool GetUserDownloadsDirectorySafe(base::FilePath* result) {
  // NOTE: Lacros overrides the path with a value from ash early in startup. See
  // crosapi::mojom::LacrosInitParams.
  CHECK(!GetDefaultPaths().downloads_dir.empty());
  *result = GetDefaultPaths().downloads_dir;
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
