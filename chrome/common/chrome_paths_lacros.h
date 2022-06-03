// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_LACROS_H_
#define CHROME_COMMON_CHROME_PATHS_LACROS_H_

namespace base {
class FilePath;
}  // namespace base

namespace chrome {

// Sets the default paths for user documents, downloads and the mount point for
// Drive. The paths are sent by ash-chrome and are set early in lacros-chrome
// startup.
void SetLacrosDefaultPaths(const base::FilePath& documents_dir,
                           const base::FilePath& downloads_dir,
                           const base::FilePath& drivefs);

// The drive fs mount point path is sent by ash-chrome, `drivefs` may be empty
// in case drive is disabled in Ash. `SetDriveFsMountPointPath()` is triggered
// in case drive availability in Ash is changed.
void SetDriveFsMountPointPath(const base::FilePath& drivefs);
// Returns false if Drive is not enabled in Ash.
bool GetDriveFsMountPointPath(base::FilePath* result);

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_PATHS_LACROS_H_
