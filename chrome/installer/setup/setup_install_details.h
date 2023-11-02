// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions for determining the installer's InstallDetails at runtime.
// See chrome/install_static/install_details.h for more information.

#ifndef CHROME_INSTALLER_SETUP_SETUP_INSTALL_DETAILS_H_
#define CHROME_INSTALLER_SETUP_SETUP_INSTALL_DETAILS_H_

#include <memory>

namespace base {
class CommandLine;
}
namespace install_static {
class PrimaryInstallDetails;
}
namespace installer {
class InitialPreferences;
}

// Creates a PrimaryInstallDetails instance for the installer and makes it the
// global InstallDetails for the process.
void InitializeInstallDetails(
    const base::CommandLine& command_line,
    const installer::InitialPreferences& initial_preferences);

// Returns a PrimaryInstallDetails instance for the installer.
std::unique_ptr<install_static::PrimaryInstallDetails> MakeInstallDetails(
    const base::CommandLine& command_line,
    const installer::InitialPreferences& initial_preferences);

#endif  // CHROME_INSTALLER_SETUP_SETUP_INSTALL_DETAILS_H_
