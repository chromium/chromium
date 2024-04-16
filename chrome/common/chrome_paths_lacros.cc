// Copyright 2020 The Chromium Authors
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
#include "chromeos/crosapi/mojom/browser_service.mojom.h"
#include "chromeos/lacros/lacros_paths.h"

namespace chrome {
namespace {

struct DefaultPaths {
  base::FilePath documents_dir;
  base::FilePath downloads_dir;
  // |drivefs| is empty if Drive is not enabled in Ash.
  base::FilePath drivefs;
  // |onedrive| is empty if Microsoft OneDrive is not mounted in Ash.
  base::FilePath onedrive;
  base::FilePath removable_media_dir;
  base::FilePath android_files_dir;
  base::FilePath linux_files_dir;
  base::FilePath share_cache_dir;
  base::FilePath preinstalled_web_app_config_dir;
  base::FilePath preinstalled_web_app_extra_config_dir;
};

DefaultPaths& GetDefaultPaths() {
  static base::NoDestructor<DefaultPaths> lacros_paths;
  return *lacros_paths;
}

}  // namespace

void SetLacrosDefaultPaths(
    const base::FilePath& documents_dir,
    const base::FilePath& downloads_dir,
    const base::FilePath& drivefs,
    const base::FilePath& onedrive,
    const base::FilePath& removable_media_dir,
    const base::FilePath& android_files_dir,
    const base::FilePath& linux_files_dir,
    const base::FilePath& ash_resources_dir,
    const base::FilePath& share_cache_dir,
    const base::FilePath& preinstalled_web_app_config_dir,
    const base::FilePath& preinstalled_web_app_extra_config_dir) {
  DCHECK(!documents_dir.empty());
  DCHECK(documents_dir.IsAbsolute());
  GetDefaultPaths().documents_dir = documents_dir;

  DCHECK(!downloads_dir.empty());
  DCHECK(downloads_dir.IsAbsolute());
  GetDefaultPaths().downloads_dir = downloads_dir;

  GetDefaultPaths().drivefs = drivefs;
  GetDefaultPaths().onedrive = onedrive;
  GetDefaultPaths().removable_media_dir = removable_media_dir;
  GetDefaultPaths().android_files_dir = android_files_dir;
  GetDefaultPaths().linux_files_dir = linux_files_dir;

  // As for ash resources path, set to chromeos::lacros_paths.
  chromeos::lacros_paths::SetAshResourcesPath(ash_resources_dir);

  GetDefaultPaths().share_cache_dir = share_cache_dir;
  GetDefaultPaths().preinstalled_web_app_config_dir =
      preinstalled_web_app_config_dir;
  GetDefaultPaths().preinstalled_web_app_extra_config_dir =
      preinstalled_web_app_extra_config_dir;
}

void SetLacrosDefaultPathsFromInitParams(
    const crosapi::mojom::DefaultPaths* default_paths) {
  // default_paths may be null on browser_tests and individual components may be
  // empty due to version skew between ash and lacros.
  if (default_paths) {
    base::FilePath drivefs_dir;
    if (default_paths->drivefs.has_value())
      drivefs_dir = default_paths->drivefs.value();
    base::FilePath onedrive_dir;
    if (default_paths->onedrive.has_value()) {
      onedrive_dir = default_paths->onedrive.value();
    }
    base::FilePath removable_media_dir;
    if (default_paths->removable_media.has_value())
      removable_media_dir = default_paths->removable_media.value();
    base::FilePath android_files_dir;
    if (default_paths->android_files.has_value())
      android_files_dir = default_paths->android_files.value();
    base::FilePath linux_files_dir;
    if (default_paths->linux_files.has_value())
      linux_files_dir = default_paths->linux_files.value();
    base::FilePath ash_resources_dir;
    if (default_paths->ash_resources.has_value())
      ash_resources_dir = default_paths->ash_resources.value();
    base::FilePath share_cache_dir;
    if (default_paths->share_cache.has_value())
      share_cache_dir = default_paths->share_cache.value();
    base::FilePath preinstalled_web_app_config_dir;
    if (default_paths->preinstalled_web_app_config.has_value()) {
      preinstalled_web_app_config_dir =
          default_paths->preinstalled_web_app_config.value();
    }
    base::FilePath preinstalled_web_app_extra_config_dir;
    if (default_paths->preinstalled_web_app_extra_config.has_value()) {
      preinstalled_web_app_extra_config_dir =
          default_paths->preinstalled_web_app_extra_config.value();
    }

    chrome::SetLacrosDefaultPaths(
        default_paths->documents, default_paths->downloads, drivefs_dir,
        onedrive_dir, removable_media_dir, android_files_dir, linux_files_dir,
        ash_resources_dir, share_cache_dir, preinstalled_web_app_config_dir,
        preinstalled_web_app_extra_config_dir);
  }
}

void SetDriveFsMountPointPath(const base::FilePath& drivefs) {
  GetDefaultPaths().drivefs = drivefs;
}

void SetOneDriveMountPointPath(const base::FilePath& onedrive) {
  GetDefaultPaths().onedrive = onedrive;
}

bool GetOneDriveMountPointPath(base::FilePath* result) {
  // NOTE: Lacros overrides the path with a value from ash early in startup. See
  // crosapi::mojom::LacrosInitParams.
  if (GetDefaultPaths().onedrive.empty()) {
    return false;
  }
  *result = GetDefaultPaths().onedrive;
  return true;
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

bool GetShareCachePath(base::FilePath* result) {
  if (GetDefaultPaths().share_cache_dir.empty())
    return false;
  *result = GetDefaultPaths().share_cache_dir;
  return true;
}

bool GetPreinstalledWebAppConfigPath(base::FilePath* result) {
  if (GetDefaultPaths().preinstalled_web_app_config_dir.empty())
    return false;
  *result = GetDefaultPaths().preinstalled_web_app_config_dir;
  return true;
}

bool GetPreinstalledWebAppExtraConfigPath(base::FilePath* result) {
  if (GetDefaultPaths().preinstalled_web_app_extra_config_dir.empty())
    return false;
  *result = GetDefaultPaths().preinstalled_web_app_extra_config_dir;
  return true;
}

}  // namespace chrome
