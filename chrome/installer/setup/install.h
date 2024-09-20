// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the specification of setup main functions.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_H_
#define CHROME_INSTALLER_SETUP_INSTALL_H_

#include <optional>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class FilePath;
class Version;
}  // namespace base

namespace installer {

struct InstallParams;
class InstallerState;
class InitialPreferences;

enum InstallShortcutOperation {
  // Create all shortcuts (potentially skipping those explicitly stated not to
  // be installed in the InstallShortcutPreferences).
  INSTALL_SHORTCUT_CREATE_ALL,
  INSTALL_SHORTCUT_FIRST = INSTALL_SHORTCUT_CREATE_ALL,
  // Create each per-user shortcut (potentially skipping those explicitly stated
  // not to be installed in the InstallShortcutPreferences), but only if the
  // system-level equivalent of that shortcut is not present on the system.
  INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL,
  // Replace all shortcuts that still exist with the most recent version of
  // each individual shortcut.
  INSTALL_SHORTCUT_REPLACE_EXISTING,
  INSTALL_SHORTCUT_LAST = INSTALL_SHORTCUT_REPLACE_EXISTING,
};

enum InstallShortcutLevel {
  // Install shortcuts for the current user only.
  INSTALL_SHORTCUT_LEVEL_FIRST = 0,
  CURRENT_USER = INSTALL_SHORTCUT_LEVEL_FIRST,
  // Install global shortcuts visible to all users. Note: the Quick Launch
  // and taskbar pin shortcuts are still installed per-user (as they have no
  // all-users version).
  ALL_USERS,
  // Update if a shortcut level is added.
  INSTALL_SHORTCUT_LEVEL_LAST = ALL_USERS,
};

// Creates chrome.VisualElementsManifest.xml in |src_path| if
// |src_path|\VisualElements exists. Returns true unless the manifest is
// supposed to be created, but fails to be.
bool CreateVisualElementsManifest(const base::FilePath& src_path,
                                  const base::Version& version);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Returns the command line to run os_update_handler, if the Windows version
// changed, std::nullopt otherwise.
// `installer_state` : used to determine file path and install level.
// `command_line` : the command line of the current process.
std::optional<base::CommandLine> GetOsUpdateHandlerCommand(
    const InstallerState& installer_state,
    const std::wstring& installed_version,
    const base::CommandLine& command_line);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Overwrites shortcuts (desktop, quick launch, and start menu) if they are
// present on the system.
// |prefs| can affect the behavior of this method through
// kDoNotCreateDesktopShortcut, kDoNotCreateQuickLaunchShortcut, and
// kAltShortcutText.
// |install_level| specifies whether to install per-user shortcuts or shortcuts
// for all users on the system (this should only be used to update legacy
// system-level installs).
// If |install_operation| is a creation command, appropriate shortcuts will be
// created even if they don't exist.
// If creating the Start menu shortcut is successful, it is also pinned to the
// taskbar.
void CreateOrUpdateShortcuts(const base::FilePath& target,
                             const InitialPreferences& prefs,
                             InstallShortcutLevel install_level,
                             InstallShortcutOperation install_operation);

// This function installs or updates a new version of Chrome. It returns
// install status (failed, new_install, updated etc).
//
// install_params: See install_params.h
// prefs: initial preferences. See chrome/installer/util/initial_preferences.h.
//
// Note: since caller unpacks Chrome to install_temp_path\source, the caller
// is responsible for cleaning up install_temp_path.
InstallStatus InstallOrUpdateProduct(
    const InstallParams& install_params,
    const base::FilePath& prefs_path,
    const installer::InitialPreferences& prefs);

// Launches a process that deletes files that belong to old versions of Chrome.
// |setup_path| is the path to the setup.exe executable to use.
void LaunchDeleteOldVersionsProcess(const base::FilePath& setup_path,
                                    const InstallerState& installer_state);

// Performs installation-related tasks following an OS upgrade.
// `installer_state` The installer state.
// `installed_version` the current version of this install.
// `setup_path` path to setup.exe
void HandleOsUpgradeForBrowser(const InstallerState& installer_state,
                               const base::Version& installed_version,
                               const base::FilePath& setup_path);

// Performs per-user installation-related tasks on Active Setup (ran on first
// login for each user post system-level Chrome install). Shortcut creation is
// skipped if the First Run beacon is present (unless `force` is set to true).
void HandleActiveSetupForBrowser(const InstallerState& installer_state,
                                 const base::FilePath& setup_path,
                                 bool force);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_H_
