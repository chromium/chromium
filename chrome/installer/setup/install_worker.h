// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the declarations of the installer functions that build
// the WorkItemList used to install the application.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_WORKER_H_
#define CHROME_INSTALLER_SETUP_INSTALL_WORKER_H_

#include <string>

#include "base/win/windows_types.h"

class WorkItemList;

namespace base {
class CommandLine;
class FilePath;
class Version;
}  // namespace base

namespace installer {

class InstallerState;
struct InstallParams;

// This method adds work items to create (or update) Chrome uninstall entry in
// either the Control Panel->Add/Remove Programs list or in the Omaha client
// state key if running under an MSI installer.
void AddUninstallShortcutWorkItems(const InstallParams& install_params,
                                   WorkItemList* install_list);

// Creates Chrome's Clients key (if not already present) and sets the new
// product version as the last step.  Also set "lang" for user-level installs.
void AddVersionKeyWorkItems(const InstallParams& install_params,
                            WorkItemList* list);

// Updates the RLZ brand code or distribution tag.  This is called by the
// installer to update deprecated, organic enterprise brand codes.
void AddUpdateBrandCodeWorkItem(const InstallerState& installer_state,
                                WorkItemList* install_list);

// Checks to see if the given brand code is one that should be updated if
// the current install is considered an enterprise install.  If so the updated
// brand code is returned, otherwise an empty string is returned.
std::wstring GetUpdatedBrandCode(const std::wstring& brand_code);

// Does forward and backword transformation of brand codes between the CBE w/o
// and CBE with CBCM codes. The `to_cbcm` parameter defines which direction is
// needed.
std::wstring TransformCloudManagementBrandCode(const std::wstring& brand_code,
                                               bool to_cbcm);

// After a successful copying of all the files, this function is called to
// do a few post install tasks:
// - Handle the case of in-use-update by updating "opv" (old version) key or
//   deleting it if not required.
// - Register any new dlls and unregister old dlls.
// - If this is an MSI install, ensures that the MSI marker is set, and sets
//   it if not.
// If these operations are successful, the function returns true, otherwise
// false.
bool AppendPostInstallTasks(const InstallParams& install_params,
                            WorkItemList* post_install_task_list);

// Builds the complete WorkItemList used to build the set of installation steps
// needed to lay down Chrome.
//
// install_params: See install_params.h
void AddInstallWorkItems(const InstallParams& install_params,
                         WorkItemList* install_list);

// Adds work items to `list` to register a COM server with the OS after deleting
// the old ones, which is used to handle the toast notification activation.
void AddNativeNotificationWorkItems(
    HKEY root,
    const base::FilePath& notification_helper_path,
    WorkItemList* list);

// Adds work items to `list` to delete all previous WER runtime exception helper
// module registrations. Registry values that fit the following pattern are
// deleted: target_path.value()\<valid version>\kWerDll
void AddOldWerHelperRegistrationCleanupItems(HKEY root,
                                             const base::FilePath& target_path,
                                             WorkItemList* list);

// Adds work items to `list` to register a WER runtime exception helper module
// in the registry. The wer module should be located at `wer_helper_path`.
void AddWerHelperRegistration(HKEY root,
                              const base::FilePath& wer_helper_path,
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
// `installer_state`).  `new_version` is the version currently being installed
// -- can be empty on uninstall.
void AddOsUpgradeWorkItems(const InstallerState& installer_state,
                           const base::FilePath& setup_path,
                           const base::Version& new_version,
                           WorkItemList* install_list);

// Adds work items to set or delete the "channel" value in `clients_key`. The
// value is set if a channel was provided to the installer via the --channel
// command line switch and deleted otherwise.
void AddChannelWorkItems(HKEY root,
                         const std::wstring& clients_key,
                         WorkItemList* list);

// Adds a best-effort item to update the "ap" value if the channel was dictated
// by --channel on the command line. This is done so that such channel changes
// are "sticky" -- once an install or update succeeds in this way, all
// subsequent update checks will be on that same channel until --channel is used
// to switch once again.
void AddChannelSelectionWorkItems(const InstallerState& installer_state,
                                  WorkItemList* list);

// Adds work items to be done when finalizing an update. This happens both
// after the executables get renamed for an in-use update or as the last steps
// for a regular update.
void AddFinalizeUpdateWorkItems(const base::Version& new_version,
                                const InstallerState& installer_state,
                                const base::FilePath& setup_path,
                                WorkItemList* list);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_WORKER_H_
