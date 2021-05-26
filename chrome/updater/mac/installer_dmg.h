// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_INSTALLER_DMG_H_
#define CHROME_UPDATER_MAC_INSTALLER_DMG_H_

#include <string>

namespace base {
class FilePath;
}

namespace updater {

enum class InstallErrors {
  // Failed to mount the DMG.
  kFailMountDmg = -1,

  // No mount point was created from the DMG, even though mounting succeeded.
  kNoMountPoint = -2,

  // Failed to find the mounted DMG path, even though mounting succeeded and a
  // mount point was created.
  kMountedDmgPathDoesNotExist = -3,

  // Failed to find a path to the install executable.
  kExecutableFilePathDoesNotExist = -4,

  // Executable path does not contain an executable file.
  kExecutablePathNotExecutable = -5
};

// Mounts the DMG specified by |dmg_file_path|. The install executable located
// at "/.install" in the mounted volume is executed, and then the DMG is
// un-mounted. Returns an error code if mounting the DMG or executing the
// executable failed.
int InstallFromDMG(const base::FilePath& dmg_file_path,
                   const base::FilePath& existence_checker_path,
                   const std::string& arguments);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_INSTALLER_DMG_H_
