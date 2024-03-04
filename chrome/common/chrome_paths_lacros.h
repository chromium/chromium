// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_LACROS_H_
#define CHROME_COMMON_CHROME_PATHS_LACROS_H_

#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace chrome {

// Sets the default paths for locations that store user controlled content
// including documents, downloads, DriveFS, removable media, ARC storage
// and Crostini's home directory. The paths are sent by ash-chrome and
// are set early in lacros-chrome startup.
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
    const base::FilePath& preinstalled_web_app_extra_config_dir);

// Sets the default paths from BrowserInitParams received from ash on startup.
void SetLacrosDefaultPathsFromInitParams(
    const crosapi::mojom::DefaultPaths* default_paths);

// The drive fs mount point path is sent by ash-chrome, `drivefs` may be empty
// in case drive is disabled in Ash. `SetDriveFsMountPointPath()` is triggered
// in case drive availability in Ash is changed.
void SetDriveFsMountPointPath(const base::FilePath& drivefs);

// Sets `result` to the the DriveFS mount point path, which is passed in by Ash
// and continually updated as it changes in Ash. The mount point path does not
// contain the trailing '/root'; this is the 'MyDrive' directory specifically.
// Returns false if Drive is not enabled in Ash.
bool GetDriveFsMountPointPath(base::FilePath* result);

// The OneDrive mount point path is sent by ash-chrome, `onedrive` may be
// empty in case OneDrive is not mounted in Ash. `SetOneDriveMountPointPath()`
// is triggered when OneDrive mount point in Ash changes.
void SetOneDriveMountPointPath(const base::FilePath& onedrive);

// Sets `result` to the the OneDrive mount point path, which is passed in by Ash
// and continually updated as it changes in Ash.
// Returns false if OneDrive is not mounted in Ash.
bool GetOneDriveMountPointPath(base::FilePath* result);

// These paths are sent by ash-chrome at Lacros startup. These return false if
// the value was not sent (eg. due to API version skew).
bool GetRemovableMediaPath(base::FilePath* result);
bool GetAndroidFilesPath(base::FilePath* result);
bool GetLinuxFilesPath(base::FilePath* result);
bool GetShareCachePath(base::FilePath* result);
bool GetPreinstalledWebAppConfigPath(base::FilePath* result);
bool GetPreinstalledWebAppExtraConfigPath(base::FilePath* result);
}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_LACROS_H_
