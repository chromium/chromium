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
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace chrome {
namespace {

struct DefaultPaths {
  base::FilePath documents_dir;
  base::FilePath downloads_dir;
  // |drivefs| is empty if Drive is not enabled in Ash.
  base::FilePath drivefs;
  base::FilePath removable_media_dir;
  base::FilePath android_files_dir;
  base::FilePath linux_files_dir;
  base::FilePath ash_resources_dir;
  base::FilePath share_cache_dir;
};

DefaultPaths& GetDefaultPaths() {
  static base::NoDestructor<DefaultPaths> lacros_paths;
  return *lacros_paths;
}

}  // namespace

void SetLacrosDefaultPaths(const base::FilePath& documents_dir,
                           const base::FilePath& downloads_dir,
                           const base::FilePath& drivefs,
                           const base::FilePath& removable_media_dir,
                           const base::FilePath& android_files_dir,
                           const base::FilePath& linux_files_dir,
                           const base::FilePath& ash_resources_dir,
                           const base::FilePath& share_cache_dir) {
  DCHECK(!documents_dir.empty());
  DCHECK(documents_dir.IsAbsolute());
  GetDefaultPaths().documents_dir = documents_dir;

  DCHECK(!downloads_dir.empty());
  DCHECK(downloads_dir.IsAbsolute());
  GetDefaultPaths().downloads_dir = downloads_dir;

  GetDefaultPaths().drivefs = drivefs;
  GetDefaultPaths().removable_media_dir = removable_media_dir;
  GetDefaultPaths().android_files_dir = android_files_dir;
  GetDefaultPaths().linux_files_dir = linux_files_dir;
  GetDefaultPaths().ash_resources_dir = ash_resources_dir;
  GetDefaultPaths().share_cache_dir = share_cache_dir;
}

void SetLacrosDefaultPathsFromInitParams(
    const crosapi::mojom::BrowserInitParams* init_params) {
  CHECK(init_params);
  // default_paths may be null on browser_tests and individual components may be
  // empty due to version skew between ash and lacros.
  if (init_params->default_paths) {
    base::FilePath drivefs_dir;
    if (init_params->default_paths->drivefs.has_value())
      drivefs_dir = init_params->default_paths->drivefs.value();
    base::FilePath removable_media_dir;
    if (init_params->default_paths->removable_media.has_value())
      removable_media_dir = init_params->default_paths->removable_media.value();
    base::FilePath android_files_dir;
    if (init_params->default_paths->android_files.has_value())
      android_files_dir = init_params->default_paths->android_files.value();
    base::FilePath linux_files_dir;
    if (init_params->default_paths->linux_files.has_value())
      linux_files_dir = init_params->default_paths->linux_files.value();
    base::FilePath ash_resources_dir;
    if (init_params->default_paths->ash_resources.has_value())
      ash_resources_dir = init_params->default_paths->ash_resources.value();
    base::FilePath share_cache_dir;
    if (init_params->default_paths->share_cache.has_value())
      share_cache_dir = init_params->default_paths->share_cache.value();

    chrome::SetLacrosDefaultPaths(
        init_params->default_paths->documents,
        init_params->default_paths->downloads, drivefs_dir, removable_media_dir,
        android_files_dir, linux_files_dir, ash_resources_dir, share_cache_dir);
  }
}

void SetDriveFsMountPointPath(const base::FilePath& drivefs) {
  GetDefaultPaths().drivefs = drivefs;
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

bool GetDriveFsMountPointPath(base::FilePath* result) {
  // NOTE: Lacros overrides the path with a value from ash early in startup. See
  // crosapi::mojom::LacrosInitParams.
  if (GetDefaultPaths().drivefs.empty())
    return false;
  *result = GetDefaultPaths().drivefs;
  return true;
}

bool GetRemovableMediaPath(base::FilePath* result) {
  if (GetDefaultPaths().removable_media_dir.empty())
    return false;
  *result = GetDefaultPaths().removable_media_dir;
  return true;
}

bool GetAndroidFilesPath(base::FilePath* result) {
  if (GetDefaultPaths().android_files_dir.empty())
    return false;
  *result = GetDefaultPaths().android_files_dir;
  return true;
}

bool GetLinuxFilesPath(base::FilePath* result) {
  if (GetDefaultPaths().linux_files_dir.empty())
    return false;
  *result = GetDefaultPaths().linux_files_dir;
  return true;
}

bool GetAshResourcesPath(base::FilePath* result) {
  if (GetDefaultPaths().ash_resources_dir.empty())
    return false;
  *result = GetDefaultPaths().ash_resources_dir;
  return true;
}

bool GetShareCachePath(base::FilePath* result) {
  if (GetDefaultPaths().share_cache_dir.empty())
    return false;
  *result = GetDefaultPaths().share_cache_dir;
  return true;
}

}  // namespace chrome
