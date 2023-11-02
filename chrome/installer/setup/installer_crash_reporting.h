// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTING_H_
#define CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTING_H_

#include <stddef.h>

namespace base {
class CommandLine;
class Version;
}  // namespace base

namespace installer {

class InstallerState;

// Sets up the crash reporting system for the installer.
void ConfigureCrashReporting(const InstallerState& installer_state);

// Sets all crash keys that are available during process startup. These do not
// vary during execution so this function will not need to be called more than
// once.
void SetInitialCrashKeys(const InstallerState& installer_state);

// Sets crash keys for the switches given in |command_line|.
void SetCrashKeysFromCommandLine(const base::CommandLine& command_line);

// Sets a crash key recording the version of the product which was present
// before the installer was run.
void SetCurrentVersionCrashKey(const base::Version& current_version);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALLER_CRASH_REPORTING_H_
