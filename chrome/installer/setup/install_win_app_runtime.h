// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_WIN_APP_RUNTIME_H_
#define CHROME_INSTALLER_SETUP_INSTALL_WIN_APP_RUNTIME_H_

namespace base {
class FilePath;
}  // namespace base

namespace installer {

// This function uses the IAppInstallManager API to install the Windows App
// Runtime package and creates a package dependency for the version folder to
// prevent the OS from removing it. If `system_install` is true, the package
// will be installed for all users.
//
// Only triggers the installation when the OS is Windows 11 version 24H2 or
// later.
void MaybeTriggerWinAppRuntimeInstallation(bool system_install,
                                           const base::FilePath& version_dir);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_WIN_APP_RUNTIME_H_
