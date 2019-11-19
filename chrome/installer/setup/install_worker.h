// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the declarations of the installer functions that build
// the WorkItemList used to install the application.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_WORKER_H_
#define CHROME_INSTALLER_SETUP_INSTALL_WORKER_H_

#include "base/strings/string16.h"
#include "base/win/windows_types.h"

class WorkItemList;

namespace base {
class CommandLine;
class FilePath;
class Version;
}

namespace installer {

class InstallationState;
class InstallerState;

// This method adds work items to create (or update) Chrome uninstall entry in
// either the Control Panel->Add/Remove Programs list or in the Omaha client
// state key if running under an MSI installer.
void AddUninstallShortcutWorkItems(const InstallerState& installer_state,
                                   const base::FilePath& setup_path,
                                   const base::Version& new_version,
                                   WorkItemList* install_list);

// Creates Chrome's Clients key (if not already present) and sets the new
// product version as the last step.  If |add_language_identifier| is true, the
// "lang" value is also set according to the currently selected translation.
void AddVersionKeyWorkItems(HKEY root,
                            const base::Version& new_version,
                            bool add_language_identifier,
                            WorkItemList* list);

// Updates the RLZ brand code or distribution tag.  This is called by the
// installer to update deprecated, organic enterprise brand codes.
void AddUpdateBrandCodeWorkItem(const InstallerState& installer_state,
                                WorkItemList* install_list);

// Checks to see if the given brand code is one that should be updated if
// the current install is considered an enterprise install.  If so the updated
// brand code is returned, otherwise an empty string is returned.
base::string16 GetUpdatedBrandCode(const base::string16& brand_code);

// After a successful copying of all the files, this function is called to
// do a few post install tasks:
// - Handle the case of in-use-update by updating "opv" (old version) key or
//   deleting it if not required.
// - Register any new dlls and unregister old dlls.
// - If this is an MSI install, ensures that the MSI marker is set, and sets
//   it if not.
// If these operations are successful, the function returns true, otherwise
// false.
// |current_version| can be NULL to indicate no Chrome is currently installed.
bool AppendPostInstallTasks(const InstallerState& installer_state,
                            const base::FilePath& setup_path,
                            const base::FilePath& src_path,
                            const base::FilePath& temp_path,
                            const base::Version* current_version,
                            const base::Version& new_version,
                            WorkItemList* post_install_task_list);

// Builds the complete WorkItemList used to build the set of installation steps
// needed to lay down Chrome.
//
// setup_path: Path to the executable (setup.exe) as it will be copied
//           to Chrome install folder after install is complete
// archive_path: Path to the archive (chrome.7z) as it will be copied
//               to Chrome install folder after install is complete
// src_path: the path that contains a complete and unpacked Chrome package
//           to be installed.
// temp_path: the path of working directory used during installation. This path
//            does not need to exist.
// |current_version| can be NULL to indicate no Chrome is currently installed.
void AddInstallWorkItems(const InstallationState& original_state,
                         const InstallerState& installer_state,
                         const base::FilePath& setup_path,
                         const base::FilePath& archive_path,
                         const base::FilePath& src_path,
                         const base::FilePath& temp_path,
                         const base::Version* current_version,
                         const base::Version& new_version,
                         WorkItemList* install_list);

// Adds work items to |list| to register a COM server with the OS after deleting
// the old ones, which is used to handle the toast notification activation.
void AddNativeNotificationWorkItems(
    HKEY root,
    const base::FilePath& notification_helper_path,
    WorkItemList* list);

void AddSetMsiMarkerWorkItem(const InstallerState& installer_state,
                             bool set,
                             WorkItemList* work_item_list);

// Adds work items to cleanup deprecated per-user registrations.
void AddCleanupDeprecatedPerUserRegistrationsWorkItems(WorkItemList* list);

// Adds Active Setup registration for sytem-level setup to be called by Windows
// on user-login post-install/update. This method should be called for
// installation only.
void AddActiveSetupWorkItems(const InstallerState& installer_state,
                             const base::Version& new_version,
                             WorkItemList* list);

// Utility method currently shared between install.cc and install_worker.cc
void AppendUninstallCommandLineFlags(const InstallerState& installer_state,
                                     base::CommandLine* uninstall_cmd);

// Adds work items to add or remove the "on-os-upgrade" command to Chrome's
// version key on the basis of the current operation (represented in
// |installer_state|).  |new_version| is the version currently being installed
// -- can be empty on uninstall.
void AddOsUpgradeWorkItems(const InstallerState& installer_state,
                           const base::FilePath& setup_path,
                           const base::Version& new_version,
                           WorkItemList* install_list);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_WORKER_H_
