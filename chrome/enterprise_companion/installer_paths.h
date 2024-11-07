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

// Determines the install directory for the app.
std::optional<base::FilePath> GetInstallDirectory();

// Searches the system for an existing installation of the app. Returns a path
// to the installed application binary, if one exists.
std::optional<base::FilePath> FindExistingInstall();

#if BUILDFLAG(IS_MAC)
// Returns the path to the system's ksadmin.
base::FilePath GetKSAdminPath();
#endif

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_INSTALLER_PATHS_H_
