// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_LAUNCHER_SUPPORT_CHROME_LAUNCHER_SUPPORT_H_
#define CHROME_INSTALLER_LAUNCHER_SUPPORT_CHROME_LAUNCHER_SUPPORT_H_

#include "base/files/file_path.h"
#include "base/version.h"

namespace chrome_launcher_support {

enum InstallationLevel {
  USER_LEVEL_INSTALLATION,
  SYSTEM_LEVEL_INSTALLATION,
};

// Returns the path to an installed chrome.exe at the specified level, if it can
// be found in the registry. If |is_sxs| is true, gets the path to the SxS
// (Canary) version of chrome.exe.
base::FilePath GetChromePathForInstallationLevel(InstallationLevel level,
                                                 bool is_sxs);

// Returns the path to an installed chrome.exe, or an empty path. Prefers a
// system-level installation to a user-level installation. Uses the registry to
// identify a Chrome installation location. If |is_sxs| is true, gets the path
// to the SxS (Canary) version of chrome.exe. The file path returned (if any) is
// guaranteed to exist.
base::FilePath GetAnyChromePath(bool is_sxs);

// Returns the version of Chrome registered in Google Update at the specified
// installation level, if it can be found in the registry.
// Note: This version number may be different from the version of Chrome that
// the user is already running or will get run when the user launches Chrome.
// If |is_sxs| is true, gets the version of the SxS (Canary) version of Chrome.
base::Version GetChromeVersionForInstallationLevel(InstallationLevel level,
                                                   bool is_sxs);

}  // namespace chrome_launcher_support

#endif  // CHROME_INSTALLER_LAUNCHER_SUPPORT_CHROME_LAUNCHER_SUPPORT_H_
