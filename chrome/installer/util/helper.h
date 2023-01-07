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

// This function returns the install path for Chrome depending on whether it's
// a system wide install or a user specific install.
// Returns the install path stored at
// Software\Google\Update\ClientState\{appguid}\UninstallString
// under HKLM if |system_install| is true, HKCU otherwise. If no path was stored
// in the registry, returns (%ProgramFiles%\[Company\]Product\Application) if
// |system_install| is true, otherwise returns user specific location
// (%LOCALAPPDATA%\[Company\]Product\Application).
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
