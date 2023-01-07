// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares Chrome uninstall related functions.

#ifndef CHROME_INSTALLER_SETUP_UNINSTALL_H_
#define CHROME_INSTALLER_SETUP_UNINSTALL_H_

#include <shlobj.h>

#include <string>

#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace installer {

class InstallerState;
struct ModifyParams;

enum DeleteResult {
  DELETE_SUCCEEDED,
  DELETE_NOT_EMPTY,
  DELETE_FAILED,
  DELETE_REQUIRES_REBOOT,
};

// Deletes |target_directory| (".../Application") and the vendor directories
// (e.g., ".../Google/Chrome") if they are empty. Returns DELETE_SUCCEEDED if
// either the directories were deleted or if they were not empty. Returns
// DELETE_FAILED if any could not be deleted due to an error.
DeleteResult DeleteChromeDirectoriesIfEmpty(
    const base::FilePath& application_directory);

// This function removes all Chrome registration related keys. It returns true
// if successful, otherwise false. The error code is set in |exit_code|.
// |root| is the registry root (HKLM|HKCU) and |browser_entry_suffix| is the
// suffix for default browser entry name in the registry (optional).
bool DeleteChromeRegistrationKeys(const InstallerState& installer_state,
                                  HKEY root,
                                  const std::wstring& browser_entry_suffix,
                                  InstallStatus* exit_code);

// Removes any legacy registry keys from earlier versions of Chrome that are no
// longer needed. This is used during autoupdate since we don't do full
// uninstalls/reinstalls to update.
void RemoveChromeLegacyRegistryKeys(const base::FilePath& chrome_exe);

// This function uninstalls a product.  Hence we came up with this awesome
// name for it.
//
// modify_params: See modify_params.h
// remove_all: Remove all shared files, registry entries as well.
// force_uninstall: Uninstall without prompting for user confirmation or
//                  any checks for Chrome running.
// cmd_line: CommandLine that contains information about the command that
//           was used to launch current uninstaller.
installer::InstallStatus UninstallProduct(const ModifyParams& modify_params,
                                          bool remove_all,
                                          bool force_uninstall,
                                          const base::CommandLine& cmd_line);

// Cleans up the installation directory after all uninstall operations have
// completed. Depending on what products are remaining, setup.exe and the
// installer archive may be deleted. Empty directories will be pruned (or
// scheduled for pruning after reboot, if necessary).
//
// target_path: Installation directory.
// setup_exe: The path to the currently running setup.exe, which will be moved
//     into a temporary directory to allow for deletion of the installation
//     directory.
// uninstall_status: the uninstall status so far (may change during invocation).
void CleanUpInstallationDirectoryAfterUninstall(
    const base::FilePath& target_path,
    const base::FilePath& setup_exe,
    InstallStatus* uninstall_status);

// Moves |setup_exe| to a temporary file, outside of the install folder.
// Also attempts to change the current directory to the TMP directory.
// On Windows, each process has a handle to its CWD. If |setup.exe|'s CWD
// happens to be within the install directory, deletion will fail as a result
// of the open handle.
bool MoveSetupOutOfInstallFolder(const base::FilePath& setup_exe);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_UNINSTALL_H_
