// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by setup.

#ifndef CHROME_INSTALLER_UTIL_HELPER_H_
#define CHROME_INSTALLER_UTIL_HELPER_H_

#include "base/files/file_path.h"

namespace installer {

class InitialPreferences;

// Returns Chrome's install path for either a per-user or a per-machine install.
// If Chrome is installed at the level specified by `system_install` (as
// indicated by the presence of a valid version value), the install path derived
// from the value in the Windows registry at
// [HKLM|HKCU]\Software\Google\Update\ClientState\{appguid}\UninstallString is
// returned if it is absolute and exists. Otherwise, the path
// ([%ProgramFiles%|%LOCALAPPDATA%]\[Company\]Product\Application) is returned,
// provided that the expanded variable yields an absolute path that exists.
base::FilePath GetChromeInstallPath(bool system_install);

// Returns a path to the directory holding chrome.exe for either a system wide
// or user specific install. The returned path will be one of:
// - The path to the current installation at |system_install|, if there is one.
// - The desired path for a new installation based on the "program_files_dir"
//   initial preference in the "distribution" dict of |prefs|, if set.
// - The default path for a new installation based on the binary's bitness.
base::FilePath GetChromeInstallPathWithPrefs(bool system_install,
                                             const InitialPreferences& prefs);
}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_HELPER_H_
