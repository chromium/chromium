// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_INSTALLER_PATHS_H_
#define CHROME_ENTERPRISE_COMPANION_INSTALLER_PATHS_H_

#include <optional>

#include "base/files/file_path.h"
#include "build/build_config.h"

// Utilities regarding installation paths for the Chrome Enterprise Companion
// App which may be depended upon by both the internal and client libraries.
namespace enterprise_companion {

// The name of the program image. E.g. "enterprise_companion.exe".
extern const char kExecutableName[];

// Determines the install directory for the app. On Windows this is dependent on
// the architecture of the program image.
std::optional<base::FilePath> GetInstallDirectory();

// Searches the system for an existing installation of the app. On Windows, it
// is possible that 32 and 64-bit installations exist. In such cases, the latter
// is returned. Returns a path to the installed application binary, if one
// exists.
std::optional<base::FilePath> FindExistingInstall();

#if BUILDFLAG(IS_MAC)
// Returns the path to the system's ksadmin.
base::FilePath GetKSAdminPath();
#endif

#if BUILDFLAG(IS_WIN)
// Returns the path to the install directory used by other builds of this
// application. That is, for 32-bit builds this returns the 64-bit install
// directory and vice versa:
// * 32-bit process on 32-bit host: nullopt
// * 32-bit process on 64-bit host: C:\Program Files\...
// * 64-bit process on 64-bit host: C:\Program Files (x86)\...
std::optional<base::FilePath> GetInstallDirectoryForAlternateArch();
#endif

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_INSTALLER_PATHS_H_
