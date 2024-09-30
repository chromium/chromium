// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the methods useful for uninstalling Chrome.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/setup/uninstall.h"

#include <windows.h>

#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/shortcut.h"
#include "build/branding_buildflags.h"
#include "chrome/chrome_elf/blocklist_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/brand_behaviors.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/launch_chrome.h"
#include "chrome/installer/setup/modify_params.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/user_hive_visitor.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/firewall_manager_win.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/registry_util.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "content/public/common/result_codes.h"
#include "rlz/lib/rlz_lib_clear.h"
#include "rlz/lib/supplementary_branding.h"

using base::win::RegKey;

namespace installer {

namespace {

// Avoid leaving behind a Temp dir.  If one exists, ask SelfCleaningTempDir to
// clean it up for us.  This may involve scheduling it for deletion after
// reboot.  Don't report that a reboot is required in this case, however.
// TODO(erikwright): Shouldn't this still lead to
// ScheduleParentAndGrandparentForDeletion?
void DeleteInstallTempDir(const base::FilePath& target_path) {
  base::FilePath temp_path(
      target_path.DirName().Append(installer::kInstallTempDir));
  if (base::DirectoryExists(temp_path)) {
    SelfCleaningTempDir temp_dir;
    if (!temp_dir.Initialize(target_path.DirName(),
                             installer::kInstallTempDir) ||
        !temp_dir.Delete()) {
      LOG(ERROR) << "Failed to delete temp dir " << temp_path.value();
    }
  }
}

// Processes uninstall WorkItems from install_worker in no-rollback-list.
void ProcessChromeWorkItems(const InstallerState& installer_state) {
  std::unique_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  work_item_list->set_log_message(
      "Cleanup OS upgrade command and deprecated per-user registrations");
  work_item_list->set_best_effort(true);
  work_item_list->set_rollback_enabled(false);
  AddOsUpgradeWorkItems(installer_state, base::FilePath(), base::Version(),
                        work_item_list.get());
  // Perform a best-effort cleanup of per-user keys. On system-level installs
  // this will only cleanup keys for the user running the uninstall but it was
  // considered that this was good enough (better than triggering Active Setup
  // for all users solely for this cleanup).
  AddCleanupDeprecatedPerUserRegistrationsWorkItems(work_item_list.get());
  work_item_list->Do();
}

void ClearRlzProductState() {
  const rlz_lib::AccessPoint points[] = {
      rlz_lib::CHROME_OMNIBOX, rlz_lib::CHROME_HOME_PAGE,
      rlz_lib::CHROME_APP_LIST, rlz_lib::NO_ACCESS_POINT};

  rlz_lib::ClearProductState(rlz_lib::CHROME, points);

  // If chrome has been reactivated, clear all events for this brand as well.
  std::wstring reactivation_brand_wide;
  if (GoogleUpdateSettings::GetReactivationBrand(&reactivation_brand_wide)) {
    std::string reactivation_brand(base::WideToASCII(reactivation_brand_wide));
    rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
    rlz_lib::ClearProductState(rlz_lib::CHROME, points);
  }
}

// Removes all files from the installer directory. Returns false in case of an
// error.
bool RemoveInstallerFiles(const base::FilePath& installer_directory) {
  base::FileEnumerator file_enumerator(
      installer_directory, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  bool success = true;

  for (base::FilePath to_delete = file_enumerator.Next(); !to_delete.empty();
       to_delete = file_enumerator.Next()) {
    VLOG(1) << "Deleting installer path " << to_delete.value();
    if (!base::DeletePathRecursively(to_delete)) {
      LOG(ERROR) << "Failed to delete path: " << to_delete.value();
      success = false;
    }
  }

  return success;
}

// Filter for processes whose base name matches and whose path starts with a
// specified prefix.
class ProcessPathPrefixFilter : public base::ProcessFilter {
 public:
  explicit ProcessPathPrefixFilter(
      const base::FilePath::StringPieceType& process_path_prefix)
      : process_path_prefix_(process_path_prefix) {}

  // base::ProcessFilter:
  bool Includes(const base::ProcessEntry& entry) const override {
    // Test if |entry|'s file path starts with the prefix we're looking for.
    base::Process process(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                        FALSE, entry.th32ProcessID));
    if (!process.IsValid())
      return false;

    DWORD path_len = MAX_PATH;
    wchar_t path_string[MAX_PATH];
    if (::QueryFullProcessImageName(process.Handle(), 0, path_string,
                                    &path_len)) {
      base::FilePath file_path(path_string);
      return base::StartsWith(file_path.value(), process_path_prefix_,
                              base::CompareCase::INSENSITIVE_ASCII);
    }
    PLOG(WARNING) << "QueryFullProcessImageName failed for PID "
                  << entry.th32ProcessID;
    return false;
  }

 private:
  const base::FilePath::StringPieceType process_path_prefix_;
};

// Kills all Chrome processes in |target_path|, immediately.
void CloseAllChromeProcesses(const base::FilePath& target_path) {
  ProcessPathPrefixFilter target_path_filter(target_path.value());
  base::CleanupProcesses(installer::kChromeExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, &target_path_filter);
}

// Updates shortcuts to |old_target_exe| that have non-empty args, making them
// target |new_target_exe| instead. The non-empty args requirement is a
// heuristic to determine whether a shortcut is "user-generated". This routine
// can only be called for user-level installs.
void RetargetUserShortcutsWithArgs(const InstallerState& installer_state,
                                   const base::FilePath& old_target_exe,
                                   const base::FilePath& new_target_exe) {
  if (installer_state.system_install()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  ShellUtil::ShellChange install_level = ShellUtil::CURRENT_USER;

  // Retarget all shortcuts that point to |old_target_exe| from all
  // ShellUtil::ShortcutLocations.
  VLOG(1) << "Retargeting shortcuts.";
  for (int location = ShellUtil::SHORTCUT_LOCATION_FIRST;
       location <= ShellUtil::SHORTCUT_LOCATION_LAST; ++location) {
    if (!ShellUtil::RetargetShortcutsWithArgs(
            static_cast<ShellUtil::ShortcutLocation>(location), install_level,
            old_target_exe, new_target_exe)) {
      LOG(WARNING) << "Failed to retarget shortcuts in ShortcutLocation: "
                   << location;
    }
  }
}

// Deletes shortcuts from Start menu, Desktop, Quick Launch, taskbar, and
// secondary tiles on the Start Screen (Win8+). Only shortcuts pointing to any
// of |target_paths| will be removed.
void DeleteShortcuts(const InstallerState& installer_state,
                     const std::vector<base::FilePath>& target_paths) {
  // The per-user shortcut for this user, if present on a system-level install,
  // has already been deleted in chrome_browser_main_win.cc::DoUninstallTasks().
  ShellUtil::ShellChange install_level = installer_state.system_install()
                                             ? ShellUtil::SYSTEM_LEVEL
                                             : ShellUtil::CURRENT_USER;
  VLOG(1) << "Deleting shortcuts.";
  ShellUtil::RemoveAllShortcuts(install_level, target_paths);
}

bool ScheduleParentAndGrandparentForDeletion(const base::FilePath& path) {
  base::FilePath parent_dir = path.DirName();
  bool ret = ScheduleFileSystemEntityForDeletion(parent_dir);
  if (!ret) {
    LOG(ERROR) << "Failed to schedule parent dir for deletion: "
               << parent_dir.value();
  } else {
    base::FilePath grandparent_dir(parent_dir.DirName());
    ret = ScheduleFileSystemEntityForDeletion(grandparent_dir);
    if (!ret) {
      LOG(ERROR) << "Failed to schedule grandparent dir for deletion: "
                 << grandparent_dir.value();
    }
  }
  return ret;
}

// Deletes the given directory if it is empty. Returns DELETE_SUCCEEDED if the
// directory is deleted, DELETE_NOT_EMPTY if it is not empty, and DELETE_FAILED
// otherwise.
DeleteResult DeleteEmptyDir(const base::FilePath& path) {
  if (!base::IsDirectoryEmpty(path))
    return DELETE_NOT_EMPTY;

  if (base::DeletePathRecursively(path))
    return DELETE_SUCCEEDED;

  LOG(ERROR) << "Failed to delete folder: " << path.value();
  return DELETE_FAILED;
}

// Get the user data directory.
base::FilePath GetUserDataDir() {
  base::FilePath path;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &path))
    return base::FilePath();
  return path;
}

// Creates a copy of the local state file and returns a path to the copy.
base::FilePath BackupLocalStateFile(const base::FilePath& user_data_dir) {
  base::FilePath backup;
  base::FilePath state_file(user_data_dir.Append(chrome::kLocalStateFilename));
  if (!base::CreateTemporaryFile(&backup))
    LOG(ERROR) << "Failed to create temporary file for Local State.";
  else
    base::CopyFile(state_file, backup);
  return backup;
}

// Deletes a given user data directory as well as the containing product
// directories if they are empty (e.g., "Google\Chrome").
DeleteResult DeleteUserDataDir(const base::FilePath& user_data_dir) {
  if (user_data_dir.empty())
    return DELETE_SUCCEEDED;

  DeleteResult result = DELETE_SUCCEEDED;
  VLOG(1) << "Deleting user profile " << user_data_dir.value();
  if (!base::DeletePathRecursively(user_data_dir)) {
    LOG(ERROR) << "Failed to delete user profile dir: "
               << user_data_dir.value();
    result = DELETE_FAILED;
  }

  const base::FilePath product_dir1(user_data_dir.DirName());
  if (!product_dir1.empty() &&
      DeleteEmptyDir(product_dir1) == DELETE_SUCCEEDED) {
    const base::FilePath product_dir2(product_dir1.DirName());
    if (!product_dir2.empty())
      DeleteEmptyDir(product_dir2);
  }

  return result;
}

DeleteResult DeleteChromeFilesAndFolders(const InstallerState& installer_state,
                                         const base::FilePath& setup_exe) {
  const base::FilePath& target_path = installer_state.target_path();
  if (target_path.empty()) {
    LOG(ERROR) << "DeleteChromeFilesAndFolders: no installation destination "
               << "path.";
    return DELETE_FAILED;  // Nothing else we can do to uninstall, so we return.
  }

  DeleteInstallTempDir(target_path);

  DeleteResult result = DELETE_SUCCEEDED;

  base::FilePath installer_directory;
  if (target_path.IsParent(setup_exe))
    installer_directory = setup_exe.DirName();

  // Enumerate all the files in target_path recursively (breadth-first).
  // We delete a file or folder unless it is a parent/child of the installer
  // directory. For parents of the installer directory, we will later recurse
  // and delete all the children (that are not also parents/children of the
  // installer directory).
  base::FileEnumerator file_enumerator(
      target_path, true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath to_delete = file_enumerator.Next(); !to_delete.empty();
       to_delete = file_enumerator.Next()) {
    if (!installer_directory.empty() &&
        (to_delete == installer_directory ||
         installer_directory.IsParent(to_delete) ||
         to_delete.IsParent(installer_directory))) {
      continue;
    }

    VLOG(1) << "Deleting install path " << to_delete.value();
    if (!base::DeletePathRecursively(to_delete)) {
      LOG(ERROR) << "Failed to delete path (1st try): " << to_delete.value();
      // Try closing any running Chrome processes and deleting files once again.
      CloseAllChromeProcesses(target_path);
      if (!base::DeletePathRecursively(to_delete)) {
        LOG(ERROR) << "Failed to delete path (2nd try): " << to_delete.value();
        result = DELETE_FAILED;
        break;
      }
    }
  }

  return result;
}

// This method checks if Chrome is currently running or if the user has
// cancelled the uninstall operation by clicking Cancel on the confirmation
// box that Chrome pops up.
InstallStatus IsChromeActiveOrUserCancelled(
    const InstallerState& installer_state) {
  int32_t exit_code = content::RESULT_CODE_NORMAL_EXIT;
  base::CommandLine options(base::CommandLine::NO_PROGRAM);
  options.AppendSwitch(installer::switches::kUninstall);

  // Here we want to save user from frustration (in case of Chrome crashes)
  // and continue with the uninstallation as long as chrome.exe process exit
  // code is NOT one of the following:
  // - UNINSTALL_CHROME_ALIVE - chrome.exe is currently running
  // - UNINSTALL_USER_CANCEL - User cancelled uninstallation
  // - HUNG - chrome.exe was killed by HuntForZombieProcesses() (until we can
  //          give this method some brains and not kill chrome.exe launched
  //          by us, we will not uninstall if we get this return code).
  VLOG(1) << "Launching Chrome to do uninstall tasks.";
  if (LaunchChromeAndWait(installer_state.target_path(), options, &exit_code)) {
    VLOG(1) << "chrome.exe launched for uninstall confirmation returned: "
            << exit_code;
    if ((exit_code == chrome::RESULT_CODE_UNINSTALL_CHROME_ALIVE) ||
        (exit_code == chrome::RESULT_CODE_UNINSTALL_USER_CANCEL) ||
        (exit_code == content::RESULT_CODE_HUNG))
      return installer::UNINSTALL_CANCELLED;

    if (exit_code == chrome::RESULT_CODE_UNINSTALL_DELETE_PROFILE)
      return installer::UNINSTALL_DELETE_PROFILE;
  } else {
    PLOG(ERROR) << "Failed to launch chrome.exe for uninstall confirmation.";
  }

  return installer::UNINSTALL_CONFIRMED;
}

bool ShouldDeleteProfile(const base::CommandLine& cmd_line,
                         InstallStatus status) {
  return status == installer::UNINSTALL_DELETE_PROFILE ||
         cmd_line.HasSwitch(installer::switches::kDeleteProfile);
}

// Removes XP-era filetype registration making Chrome the default browser.
// MSDN (see http://msdn.microsoft.com/library/windows/desktop/cc144148.aspx)
// tells us not to do this, but certain applications break following
// uninstallation if we don't.
void RemoveFiletypeRegistration(const InstallerState& installer_state,
                                HKEY root,
                                const std::wstring& browser_entry_suffix) {
  std::wstring classes_path(ShellUtil::kRegClasses);
  classes_path.push_back(base::FilePath::kSeparators[0]);

  const std::wstring prog_id(install_static::GetBrowserProgIdPrefix() +
                             browser_entry_suffix);

  // Delete each filetype association if it references this Chrome.  Take care
  // not to delete the association if it references a system-level install of
  // Chrome (only a risk if the suffix is empty).  Don't delete the whole key
  // since other apps may have stored data there.
  std::vector<const wchar_t*> cleared_assocs;
  if (installer_state.system_install() || !browser_entry_suffix.empty() ||
      !base::win::RegKey(HKEY_LOCAL_MACHINE, (classes_path + prog_id).c_str(),
                         KEY_QUERY_VALUE)
           .Valid()) {
    ValueEquals prog_id_pred(prog_id);
    for (const wchar_t* const* filetype =
             &ShellUtil::kPotentialFileAssociations[0];
         *filetype != nullptr; ++filetype) {
      if (DeleteRegistryValueIf(
              root, (classes_path + *filetype).c_str(), WorkItem::kWow64Default,
              nullptr, prog_id_pred) == ConditionalDeleteResult::DELETED) {
        cleared_assocs.push_back(*filetype);
      }
    }
  }

  // For all filetype associations in HKLM that have just been removed, attempt
  // to restore some reasonable value.  We have no definitive way of knowing
  // what handlers are the most appropriate, so we use a fixed mapping based on
  // the default values for a fresh install of Windows.
  if (root == HKEY_LOCAL_MACHINE) {
    std::wstring assoc;
    base::win::RegKey key;

    for (size_t i = 0; i < cleared_assocs.size(); ++i) {
      const wchar_t* replacement_prog_id = nullptr;
      assoc.assign(cleared_assocs[i]);

      // Inelegant, but simpler than a pure data-driven approach.
      if (assoc == L".htm" || assoc == L".html")
        replacement_prog_id = L"htmlfile";
      else if (assoc == L".xht" || assoc == L".xhtml")
        replacement_prog_id = L"xhtmlfile";

      if (!replacement_prog_id) {
        LOG(WARNING) << "No known replacement ProgID for " << assoc
                     << " files.";
      } else if (key.Open(HKEY_LOCAL_MACHINE,
                          (classes_path + replacement_prog_id).c_str(),
                          KEY_QUERY_VALUE) == ERROR_SUCCESS &&
                 (key.Open(HKEY_LOCAL_MACHINE, (classes_path + assoc).c_str(),
                           KEY_SET_VALUE) != ERROR_SUCCESS ||
                  key.WriteValue(nullptr, replacement_prog_id) !=
                      ERROR_SUCCESS)) {
        // The replacement ProgID is registered on the computer but the attempt
        // to set it for the filetype failed.
        LOG(ERROR) << "Failed to restore system-level filetype association "
                   << assoc << " = " << replacement_prog_id;
      }
    }
  }
}

bool DeleteUserRegistryKeys(const std::vector<const std::wstring*>* key_paths,
                            const wchar_t* user_sid,
                            base::win::RegKey* key) {
  for (const auto* key_path : *key_paths) {
    LONG result = key->DeleteKey(key_path->c_str());
    if (result == ERROR_SUCCESS) {
      VLOG(1) << "Deleted " << user_sid << "\\" << *key_path;
    } else if (result != ERROR_FILE_NOT_FOUND) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed deleting " << user_sid << "\\" << *key_path;
    }
  }
  return true;
}

// Removes Active Setup entries from the registry. This cannot be done through
// a work items list as usual because of different paths based on conditionals,
// but otherwise respects the no rollback/best effort uninstall mentality.
// This will only apply for system-level installs of Chrome/Chromium and will be
// a no-op for all other types of installs.
void UninstallActiveSetupEntries(const InstallerState& installer_state) {
  VLOG(1) << "Uninstalling registry entries for Active Setup.";

  if (!installer_state.system_install()) {
    VLOG(1) << "No Active Setup processing to do for user-level install.";
    return;
  }

  const std::wstring active_setup_path(install_static::GetActiveSetupPath());
  DeleteRegistryKey(HKEY_LOCAL_MACHINE, active_setup_path,
                    WorkItem::kWow64Default);

  // Windows leaves keys behind in HKCU\\Software\\(Wow6432Node\\)?Microsoft\\
  //     Active Setup\\Installed Components\\{guid}
  // for every user that logged in since system-level Chrome was installed. This
  // is a problem because Windows compares the value of the Version subkey in
  // there with the value of the Version subkey in the matching HKLM entries
  // before running Chrome's Active Setup so if Chrome was to be
  // uninstalled/reinstalled by an admin, some users may not go through Active
  // Setup again as desired.
  //
  // It is however very hard to delete those values as the registry hives for
  // other users are not loaded by default under HKEY_USERS (unless a user is
  // logged on or has a process impersonating them).
  //
  // Following our best effort uninstall practices, try to delete the value in
  // all users hives. If a given user's hive is not loaded, try to load it to
  // proceed with the deletion (failure to do so is ignored).

  // Windows automatically adds Wow6432Node when creating/deleting the HKLM key,
  // but doesn't seem to do so when manually deleting the user-level keys it
  // created.
  std::wstring alternate_active_setup_path(active_setup_path);
  alternate_active_setup_path.insert(std::size("Software\\") - 1,
                                     L"Wow6432Node\\");

  VLOG(1) << "Uninstall per-user Active Setup keys.";
  std::vector<const std::wstring*> paths = {&active_setup_path,
                                            &alternate_active_setup_path};
  VisitUserHives(
      base::BindRepeating(&DeleteUserRegistryKeys, base::Unretained(&paths)));
}

// Removes the persistent blocklist state for the current user.  Note: this will
// not remove the state for users other than the one uninstalling Chrome on a
// system-level install (http://crbug.com/388725). Doing so would require
// extracting the per-user registry hive iteration from
// UninstallActiveSetupEntries so that it could service multiple tasks.
void RemoveBlocklistState() {
  DeleteRegistryKey(HKEY_CURRENT_USER,
                    install_static::GetRegistryPath().append(
                        blocklist::kRegistryBeaconKeyName),
                    0);  // wow64_access
}

// Removes the browser's persistent state in the Windows registry for the
// current user. Note: this will not remove the state for users other than the
// one uninstalling Chrome on a system-level install; see RemoveBlocklistState
// for details.
void RemoveDistributionRegistryState() {
  // Delete the contents of the distribution key except for those parts used by
  // outsiders to configure Chrome.
  DeleteRegistryKeyPartial(HKEY_CURRENT_USER, install_static::GetRegistryPath(),
                           {L"Extensions", L"NativeMessagingHosts"});
}

// Deletes {root}\Software\Classes\{prog_id} registry key.
bool DeleteProgIdFromSoftwareClasses(HKEY root, const std::wstring& prog_id) {
  std::wstring reg_prog_id(ShellUtil::kRegClasses);
  reg_prog_id.push_back(base::FilePath::kSeparators[0]);
  reg_prog_id.append(prog_id);
  return DeleteRegistryKey(root, reg_prog_id, WorkItem::kWow64Default);
}

}  // namespace

DeleteResult DeleteChromeDirectoriesIfEmpty(
    const base::FilePath& application_directory) {
  DeleteResult result(DeleteEmptyDir(application_directory));
  if (result == DELETE_SUCCEEDED) {
    // Now check and delete if the parent directories are empty
    // For example Google\Chrome or Chromium
    const base::FilePath product_directory(application_directory.DirName());
    if (!product_directory.empty()) {
      result = DeleteEmptyDir(product_directory);
      if (result == DELETE_SUCCEEDED) {
        const base::FilePath vendor_directory(product_directory.DirName());
        if (!vendor_directory.empty())
          result = DeleteEmptyDir(vendor_directory);
      }
    }
  }
  if (result == DELETE_NOT_EMPTY)
    result = DELETE_SUCCEEDED;
  return result;
}

void DeleteWerRegistryKeys(const installer::InstallerState& installer_state) {
  // Delete WER runtime exception helper module dll registry entries
  // for currently uninstalled Chrome version and all previous versions if any.
  std::unique_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  AddOldWerHelperRegistrationCleanupItems(installer_state.root_key(),
                                          installer_state.target_path(),
                                          work_item_list.get());
  work_item_list->Do();
}

bool DeleteChromeRegistrationKeys(const InstallerState& installer_state,
                                  HKEY root,
                                  const std::wstring& browser_entry_suffix,
                                  InstallStatus* exit_code) {
  DCHECK(exit_code);
  const base::FilePath chrome_exe(
      installer_state.target_path().Append(kChromeExe));

  // Delete {root}\Software\Classes\ChromeHTML.
  const std::wstring html_prog_id(install_static::GetBrowserProgIdPrefix() +
                                  browser_entry_suffix);
  DeleteProgIdFromSoftwareClasses(root, html_prog_id);

  // Delete {root}\Software\Classes\Chrome.

  // Append the requested suffix manually here as ShellUtil::GetBrowserModelId
  // would try to figure out the currently installed suffix.
  const std::wstring chrome_prog_id(install_static::GetBaseAppId() +
                                    browser_entry_suffix);
  DeleteProgIdFromSoftwareClasses(root, chrome_prog_id);

  // TODO(crbug.com/40384442): Delete ChromePDF ProgId once support for
  // PDF docs has landed.

  // Delete Software\Classes\CLSID\|toast_activator_clsid|.
  const std::wstring toast_activator_reg_path =
      InstallUtil::GetToastActivatorRegistryPath();
  if (!toast_activator_reg_path.empty()) {
    DeleteRegistryKey(root, toast_activator_reg_path, WorkItem::kWow64Default);
  } else {
    LOG(DFATAL) << "Cannot retrieve the toast activator registry path";
  }

  if (installer_state.system_install()) {
    if (!InstallServiceWorkItem::DeleteService(
            install_static::GetElevationServiceName(),
            install_static::GetClientStateKeyPath(),
            {install_static::GetElevatorClsid()},
            {install_static::GetElevatorIid()})) {
      LOG(WARNING) << "Failed to delete "
                   << install_static::GetElevationServiceName();
    }
    if (!InstallServiceWorkItem::DeleteService(
            install_static::GetTracingServiceName(),
            install_static::GetClientStateKeyPath(),
            {install_static::GetTracingServiceClsid()},
            {install_static::GetTracingServiceIid()})) {
      LOG(WARNING) << "Failed to delete "
                   << install_static::GetTracingServiceName();
    }
  }

  // Delete all Start Menu Internet registrations that refer to this Chrome.
  {
    using base::win::RegistryKeyIterator;
    ProgramCompare open_command_pred(chrome_exe);
    std::wstring client_name;
    std::wstring client_key;
    std::wstring open_key;
    for (RegistryKeyIterator iter(root, ShellUtil::kRegStartMenuInternet);
         iter.Valid(); ++iter) {
      client_name.assign(iter.Name());
      client_key.assign(ShellUtil::kRegStartMenuInternet)
          .append(1, L'\\')
          .append(client_name);
      open_key.assign(client_key).append(ShellUtil::kRegShellOpen);
      if (DeleteRegistryKeyIf(
              root, client_key, open_key, WorkItem::kWow64Default, nullptr,
              open_command_pred) != ConditionalDeleteResult::NOT_FOUND) {
        // Delete the default value of SOFTWARE\Clients\StartMenuInternet if it
        // references this Chrome (i.e., if it was made the default browser).
        DeleteRegistryValueIf(root, ShellUtil::kRegStartMenuInternet,
                              WorkItem::kWow64Default, nullptr,
                              ValueEquals(client_name));
        // Also delete the value for the default user if we're operating in
        // HKLM.
        if (root == HKEY_LOCAL_MACHINE) {
          DeleteRegistryValueIf(HKEY_USERS,
                                std::wstring(L".DEFAULT\\")
                                    .append(ShellUtil::kRegStartMenuInternet)
                                    .c_str(),
                                WorkItem::kWow64Default, nullptr,
                                ValueEquals(client_name));
        }
      }
    }
  }

  // Delete Software\RegisteredApplications\Chromium
  DeleteRegistryValue(
      root, ShellUtil::kRegRegisteredApplications, WorkItem::kWow64Default,
      install_static::GetBaseAppName().append(browser_entry_suffix));

  // Delete the App Paths and Applications keys that let Explorer find Chrome:
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ee872121
  std::wstring app_key(ShellUtil::kRegClasses);
  app_key.push_back(base::FilePath::kSeparators[0]);
  app_key.append(L"Applications");
  app_key.push_back(base::FilePath::kSeparators[0]);
  app_key.append(installer::kChromeExe);
  DeleteRegistryKey(root, app_key, WorkItem::kWow64Default);

  std::wstring app_path_key(ShellUtil::kAppPathsRegistryKey);
  app_path_key.push_back(base::FilePath::kSeparators[0]);
  app_path_key.append(installer::kChromeExe);
  DeleteRegistryKey(root, app_path_key, WorkItem::kWow64Default);

  // Cleanup OpenWithList and OpenWithProgids:
  // http://msdn.microsoft.com/en-us/library/bb166549
  std::wstring file_assoc_key;
  std::wstring open_with_list_key;
  std::wstring open_with_progids_key;
  for (int i = 0; ShellUtil::kPotentialFileAssociations[i] != nullptr; ++i) {
    file_assoc_key.assign(ShellUtil::kRegClasses);
    file_assoc_key.push_back(base::FilePath::kSeparators[0]);
    file_assoc_key.append(ShellUtil::kPotentialFileAssociations[i]);
    file_assoc_key.push_back(base::FilePath::kSeparators[0]);

    open_with_list_key.assign(file_assoc_key);
    open_with_list_key.append(L"OpenWithList");
    open_with_list_key.push_back(base::FilePath::kSeparators[0]);
    open_with_list_key.append(installer::kChromeExe);
    DeleteRegistryKey(root, open_with_list_key, WorkItem::kWow64Default);

    open_with_progids_key.assign(file_assoc_key);
    open_with_progids_key.append(ShellUtil::kRegOpenWithProgids);
    DeleteRegistryValue(root, open_with_progids_key, WorkItem::kWow64Default,
                        html_prog_id);
  }

  // Cleanup in case Chrome had been made the default browser.

  // Delete the default value of SOFTWARE\Clients\StartMenuInternet if it
  // references this Chrome.  Do this explicitly here for the case where HKCU is
  // being processed; the iteration above will have no hits since registration
  // lives in HKLM.
  DeleteRegistryValueIf(
      root, ShellUtil::kRegStartMenuInternet, WorkItem::kWow64Default, nullptr,
      ValueEquals(
          install_static::GetBaseAppName().append(browser_entry_suffix)));

  // Delete each protocol association if it references this Chrome.
  ProgramCompare open_command_pred(chrome_exe);
  std::wstring parent_key(ShellUtil::kRegClasses);
  parent_key.push_back(base::FilePath::kSeparators[0]);
  const std::wstring::size_type base_length = parent_key.size();
  std::wstring child_key;
  for (const wchar_t* const* proto =
           &ShellUtil::kPotentialProtocolAssociations[0];
       *proto != nullptr; ++proto) {
    parent_key.resize(base_length);
    parent_key.append(*proto);
    child_key.assign(parent_key).append(ShellUtil::kRegShellOpen);
    DeleteRegistryKeyIf(root, parent_key, child_key, WorkItem::kWow64Default,
                        nullptr, open_command_pred);
  }

  RemoveFiletypeRegistration(installer_state, root, browser_entry_suffix);

  *exit_code = installer::UNINSTALL_SUCCESSFUL;
  return true;
}

void RemoveChromeLegacyRegistryKeys(const base::FilePath& chrome_exe) {
  // We used to register Chrome to handle crx files, but this turned out
  // to be not worth the hassle. Remove these old registry entries if
  // they exist. See: http://codereview.chromium.org/210007

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const wchar_t kChromeExtProgId[] = L"ChromeExt";
#else
  const wchar_t kChromeExtProgId[] = L"ChromiumExt";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING

  HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
  for (size_t i = 0; i < std::size(roots); ++i) {
    std::wstring suffix;
    if (roots[i] == HKEY_LOCAL_MACHINE)
      suffix = ShellUtil::GetCurrentInstallationSuffix(chrome_exe);

    // Delete Software\Classes\ChromeExt,
    std::wstring ext_prog_id(ShellUtil::kRegClasses);
    ext_prog_id.push_back(base::FilePath::kSeparators[0]);
    ext_prog_id.append(kChromeExtProgId);
    ext_prog_id.append(suffix);
    DeleteRegistryKey(roots[i], ext_prog_id, WorkItem::kWow64Default);

    // Delete Software\Classes\.crx,
    std::wstring ext_association(ShellUtil::kRegClasses);
    ext_association.append(L"\\");
    ext_association.append(L".crx");
    DeleteRegistryKey(roots[i], ext_association, WorkItem::kWow64Default);
  }
}

void UninstallFirewallRules(const base::FilePath& chrome_exe) {
  std::unique_ptr<FirewallManager> manager =
      FirewallManager::Create(chrome_exe);
  if (manager)
    manager->RemoveFirewallRules();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Run os_update_handler.exe with --uninstall switch, and system-level, if
// install is a system install. Waits for os_update_handler.exe process to exit
// so that the exe file can be deleted. `installer_dir` is the setup.exe
// location and os_update_handler.exe is in its parent dir.
void UninstallOsUpdateHandler(const base::FilePath& installer_dir,
                              const InstallerState& installer_state) {
  const base::FilePath os_update_handler_exe =
      installer_dir.DirName().Append(installer::kOsUpdateHandlerExe);
  constexpr base::TimeDelta kOsUpdateUninstallTimeout = base::Seconds(5);
  base::CommandLine uninstall_cmd(os_update_handler_exe);
  uninstall_cmd.AppendSwitch(installer::switches::kUninstall);
  if (installer_state.system_install()) {
    uninstall_cmd.AppendSwitch(installer::switches::kSystemLevel);
  }
  const std::wstring cmd_string = uninstall_cmd.GetCommandLineString();
  VLOG(1) << "Launching: " << cmd_string;
  const base::Process process = base::LaunchProcess(uninstall_cmd, {});
  int exit_code = 0;
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch (" << cmd_string << ")";
  } else if (!process.WaitForExitWithTimeout(kOsUpdateUninstallTimeout,
                                             &exit_code)) {
    // The GetExitCodeProcess failed or timed-out.
    LOG(ERROR) << "Command (" << cmd_string << ") is taking more than "
               << kOsUpdateUninstallTimeout.InMilliseconds()
               << " milliseconds to complete. Terminating it.";
    process.Terminate(0, /*wait=*/true);
  } else if (exit_code != 0) {
    LOG(ERROR) << "Command (" << cmd_string << ") exited with code "
               << exit_code;
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

InstallStatus UninstallProduct(const ModifyParams& modify_params,
                               bool remove_all,
                               bool force_uninstall,
                               const base::CommandLine& cmd_line) {
  const InstallationState& original_state = *modify_params.installation_state;
  const InstallerState& installer_state = *modify_params.installer_state;
  const base::FilePath& setup_exe = *modify_params.setup_path;

  const ProductState* const product_state =
      original_state.GetProductState(installer_state.system_install());
  if (product_state) {
    VLOG(1) << "version on the system: "
            << product_state->version().GetString();
  } else if (!force_uninstall) {
    LOG(ERROR) << "Chrome not found for uninstall.";
    return installer::CHROME_NOT_INSTALLED;
  }

  InstallStatus status = installer::UNINSTALL_CONFIRMED;
  const base::FilePath chrome_exe(
      installer_state.target_path().Append(installer::kChromeExe));

  VLOG(1) << "UninstallProduct: Chrome";

  if (force_uninstall) {
    // Since --force-uninstall command line option is used, we are going to
    // do silent uninstall. Try to close all running Chrome instances.
    CloseAllChromeProcesses(installer_state.target_path());
  } else {
    // no --force-uninstall so lets show some UI dialog boxes.
    status = IsChromeActiveOrUserCancelled(installer_state);
    if (status != installer::UNINSTALL_CONFIRMED &&
        status != installer::UNINSTALL_DELETE_PROFILE)
      return status;

    const std::wstring suffix(
        ShellUtil::GetCurrentInstallationSuffix(chrome_exe));

    // Check if we need admin rights to cleanup HKLM (the conditions for
    // requiring a cleanup are the same as the conditions to do the actual
    // cleanup where DeleteChromeRegistrationKeys() is invoked for
    // HKEY_LOCAL_MACHINE below). If we do, try to launch another uninstaller
    // (silent) in elevated mode to do HKLM cleanup.
    // And continue uninstalling in the current process also to do HKCU cleanup.
    if (remove_all &&
        ShellUtil::QuickIsChromeRegisteredInHKLM(chrome_exe, suffix) &&
        !::IsUserAnAdmin() &&
        !cmd_line.HasSwitch(installer::switches::kRunAsAdmin)) {
      base::CommandLine new_cmd(base::CommandLine::NO_PROGRAM);
      new_cmd.AppendArguments(cmd_line, true);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      new_cmd.AppendSwitch(installer::switches::kRunAsAdmin);
      // Append --remove-chrome-registration to remove registry keys only.
      new_cmd.AppendSwitch(installer::switches::kRemoveChromeRegistration);
      if (!suffix.empty()) {
        new_cmd.AppendSwitchNative(
            installer::switches::kRegisterChromeBrowserSuffix, suffix);
      }
      DWORD exit_code = installer::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(new_cmd, &exit_code);
    }
  }

  // Chrome is not in use so lets uninstall Chrome by deleting various files
  // and registry entries. Here we will just make best effort and keep going
  // in case of errors.
  ClearRlzProductState();

  auto_launch_util::DisableBackgroundStartAtLogin();

  base::FilePath chrome_proxy_exe(
      installer_state.target_path().Append(installer::kChromeProxyExe));

  // If user-level chrome is self-destructing as a result of encountering a
  // system-level chrome, retarget owned non-default shortcuts (app shortcuts,
  // profile shortcuts, etc.) to the system-level chrome.
  if (cmd_line.HasSwitch(installer::switches::kSelfDestruct) &&
      !installer_state.system_install()) {
    VLOG(1) << "Retargeting user-generated Chrome shortcuts.";
    const base::FilePath system_install_path(
        GetInstalledDirectory(/*system_install=*/true));
    if (system_install_path.empty()) {
      LOG(ERROR) << "Retarget failed: system-level Chrome install directory "
                    "not found.";
    } else {
      const base::FilePath system_chrome_path(
          system_install_path.Append(installer::kChromeExe));
      if (base::PathExists(system_chrome_path)) {
        RetargetUserShortcutsWithArgs(installer_state, chrome_exe,
                                      system_chrome_path);
      } else {
        LOG(ERROR) << "Retarget failed: system-level Chrome not found.";
      }
    }

    // Retarget owned app shortcuts to the system-level chrome_proxy.
    const base::FilePath system_chrome_proxy_path(
        system_install_path.Append(installer::kChromeProxyExe));
    VLOG(1) << "Retargeting user-generated Chrome Proxy shortcuts.";
    if (base::PathExists(system_chrome_proxy_path)) {
      RetargetUserShortcutsWithArgs(installer_state, chrome_proxy_exe,
                                    system_chrome_proxy_path);
    } else {
      LOG(ERROR) << "Retarget failed: system-level Chrome Proxy not found.";
    }
  }

  DeleteShortcuts(installer_state, {chrome_exe, std::move(chrome_proxy_exe)});

  // Delete the registry keys (Uninstall key and Version key).
  HKEY reg_root = installer_state.root_key();

  // Note that we must retrieve the distribution-specific data before deleting
  // the browser's Clients key.
  std::wstring distribution_data(GetDistributionData());

  // Remove Control Panel uninstall link.
  DeleteRegistryKey(reg_root, install_static::GetUninstallRegistryPath(),
                    KEY_WOW64_32KEY);

  // Remove Omaha product key.
  DeleteRegistryKey(reg_root, install_static::GetClientsKeyPath(),
                    KEY_WOW64_32KEY);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  UninstallOsUpdateHandler(setup_exe.DirName(), installer_state);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Also try to delete the MSI value in the ClientState key (it might not be
  // there). This is due to a Google Update behaviour where an uninstall and a
  // rapid reinstall might result in stale values from the old ClientState key
  // being picked up on reinstall.
  DeleteRegistryValue(installer_state.root_key(),
                      install_static::GetClientStateKeyPath(), KEY_WOW64_32KEY,
                      google_update::kRegMSIField);

  InstallStatus ret = installer::UNKNOWN_STATUS;

  const std::wstring suffix(
      ShellUtil::GetCurrentInstallationSuffix(chrome_exe));

  // Remove all Chrome registration keys.
  // Registration data is put in HKCU for both system level and user level
  // installs.
  DeleteChromeRegistrationKeys(installer_state, HKEY_CURRENT_USER, suffix,
                               &ret);

  // If the user's Chrome is registered with a suffix: it is possible that old
  // unsuffixed registrations were left in HKCU (e.g. if this install was
  // previously installed with no suffix in HKCU (old suffix rules if the user
  // is not an admin (or declined UAC at first run)) and later had to be
  // suffixed when fully registered in HKLM (e.g. when later making Chrome
  // default through the UI)).
  // Remove remaining HKCU entries with no suffix if any.
  if (!suffix.empty()) {
    DeleteChromeRegistrationKeys(installer_state, HKEY_CURRENT_USER,
                                 std::wstring(), &ret);

    // For similar reasons it is possible in very few installs (from
    // 21.0.1180.0 and fixed shortly after) to be installed with the new-style
    // suffix, but have some old-style suffix registrations left behind.
    std::wstring old_style_suffix;
    if (ShellUtil::GetOldUserSpecificRegistrySuffix(&old_style_suffix) &&
        suffix != old_style_suffix) {
      DeleteChromeRegistrationKeys(installer_state, HKEY_CURRENT_USER,
                                   old_style_suffix, &ret);
    }
  }

  // Chrome is registered in HKLM for all system-level installs and for
  // user-level installs for which Chrome has been made the default browser.
  // Always remove the HKLM registration for system-level installs. For
  // user-level installs, only remove it if both: 1) this uninstall isn't a self
  // destruct following the installation of a system-level Chrome (because the
  // system-level Chrome owns the HKLM registration now), and 2) this user has
  // made Chrome their default browser (i.e. has shell integration entries
  // registered with |suffix| (note: |suffix| will be the empty string if
  // required as it is obtained by GetCurrentInstallationSuffix() above)).
  // TODO(gab): This can still leave parts of a suffixed install behind. To be
  // able to remove them we would need to be able to remove only suffixed
  // entries (as it is now some of the registry entries (e.g. App Paths) are
  // unsuffixed; thus removing suffixed installs is prohibited in HKLM if
  // !|remove_all| for now).
  if (installer_state.system_install() ||
      (remove_all &&
       ShellUtil::QuickIsChromeRegisteredInHKLM(chrome_exe, suffix))) {
    DeleteChromeRegistrationKeys(installer_state, HKEY_LOCAL_MACHINE, suffix,
                                 &ret);
  }

  DeleteWerRegistryKeys(installer_state);

  ProcessChromeWorkItems(installer_state);

  UninstallActiveSetupEntries(installer_state);

  UninstallFirewallRules(chrome_exe);

  RemoveBlocklistState();

  // Notify the shell that associations have changed since Chrome was likely
  // unregistered.
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  // Remove the event log provider registration as we are going to delete
  // the file which serves the resources anyways.
  DeRegisterEventLogProvider();

  // Delete shared registry keys as well (these require admin rights) if
  // remove_all option is specified.
  if (remove_all) {
    if (installer_state.system_install()) {
      // Delete media player registry key that exists only in HKLM.
      std::wstring reg_path(installer::kMediaPlayerRegPath);
      reg_path.push_back(base::FilePath::kSeparators[0]);
      reg_path.append(installer::kChromeExe);
      DeleteRegistryKey(HKEY_LOCAL_MACHINE, reg_path, WorkItem::kWow64Default);
    }
  }

  // Finally delete all the files from Chrome folder after moving setup.exe
  // and the user's Local State to a temp location.
  bool delete_profile = ShouldDeleteProfile(cmd_line, status);
  ret = installer::UNINSTALL_SUCCESSFUL;

  base::FilePath user_data_dir(GetUserDataDir());
  base::FilePath backup_state_file;
  if (!user_data_dir.empty()) {
    backup_state_file = BackupLocalStateFile(user_data_dir);
  } else {
    LOG(ERROR) << "Could not retrieve the user's profile directory.";
    ret = installer::UNINSTALL_FAILED;
    delete_profile = false;
  }

  DeleteResult delete_result = DeleteChromeFilesAndFolders(
      installer_state, base::MakeAbsoluteFilePath(setup_exe));
  if (delete_result == DELETE_FAILED)
    ret = installer::UNINSTALL_FAILED;
  else if (delete_result == DELETE_REQUIRES_REBOOT)
    ret = installer::UNINSTALL_REQUIRES_REBOOT;

  if (delete_profile) {
    DeleteUserDataDir(user_data_dir);
    RemoveDistributionRegistryState();
  }

  if (!force_uninstall && product_state) {
    VLOG(1) << "Uninstallation complete. Launching post-uninstall operations.";
    DoPostUninstallOperations(product_state->version(), backup_state_file,
                              distribution_data);
  }

  // Try and delete the preserved local state once the post-install
  // operations are complete.
  if (!backup_state_file.empty())
    base::DeleteFile(backup_state_file);

  return ret;
}

void CleanUpInstallationDirectoryAfterUninstall(
    const base::FilePath& target_path,
    const base::FilePath& setup_exe,
    InstallStatus* uninstall_status) {
  if (*uninstall_status != UNINSTALL_SUCCESSFUL &&
      *uninstall_status != UNINSTALL_REQUIRES_REBOOT) {
    return;
  }
  if (target_path.empty()) {
    LOG(ERROR) << "No installation destination path.";
    *uninstall_status = UNINSTALL_FAILED;
    return;
  }
  if (!target_path.IsParent(base::MakeAbsoluteFilePath(setup_exe))) {
    VLOG(1) << "setup.exe is not in target path. Skipping installer cleanup.";
    return;
  }
  base::FilePath install_directory(setup_exe.DirName());

  VLOG(1) << "Removing all installer files.";

  // In order to be able to remove the folder in which we're running, we need to
  // move setup.exe out of the install folder.
  // TODO(tommi): What if the temp folder is on a different volume?
  MoveSetupOutOfInstallFolder(setup_exe);

  // Remove files from "...\<product>\Application\<version>\Installer"
  if (!RemoveInstallerFiles(install_directory)) {
    *uninstall_status = UNINSTALL_FAILED;
    return;
  }

  // Try to remove the empty directory hierarchy.

  // Delete "...\<product>\Application\<version>\Installer"
  if (DeleteEmptyDir(install_directory) != DELETE_SUCCEEDED) {
    *uninstall_status = UNINSTALL_FAILED;
    return;
  }

  // Delete "...\<product>\Application\<version>"
  DeleteResult delete_result = DeleteEmptyDir(install_directory.DirName());
  if (delete_result == DELETE_FAILED ||
      (delete_result == DELETE_NOT_EMPTY &&
       *uninstall_status != UNINSTALL_REQUIRES_REBOOT)) {
    *uninstall_status = UNINSTALL_FAILED;
    return;
  }

  if (*uninstall_status == UNINSTALL_REQUIRES_REBOOT) {
    // Delete the Application directory at reboot if empty.
    ScheduleFileSystemEntityForDeletion(target_path);

    // If we need a reboot to continue, schedule the parent directories for
    // deletion unconditionally. If they are not empty, the session manager
    // will not delete them on reboot.
    ScheduleParentAndGrandparentForDeletion(target_path);
  } else if (DeleteChromeDirectoriesIfEmpty(target_path) == DELETE_FAILED) {
    *uninstall_status = UNINSTALL_FAILED;
  }
}

bool MoveSetupOutOfInstallFolder(const base::FilePath& setup_exe) {
  // The list of files which setup.exe depends on at runtime. Typically this is
  // solely setup.exe itself, but in component builds this also includes the
  // DLLs installed by setup.exe.
  std::vector<base::FilePath> setup_files;
  setup_files.push_back(setup_exe);
#if defined(COMPONENT_BUILD)
  base::FileEnumerator file_enumerator(setup_exe.DirName(), false,
                                       base::FileEnumerator::FILES, L"*.dll");
  for (base::FilePath setup_dll = file_enumerator.Next(); !setup_dll.empty();
       setup_dll = file_enumerator.Next()) {
    setup_files.push_back(setup_dll);
  }
#endif  // defined(COMPONENT_BUILD)

  base::FilePath tmp_dir;
  base::FilePath temp_file;
  if (!(::IsUserAnAdmin()
            ? base::PathService::Get(base::DIR_SYSTEM_TEMP, &tmp_dir)
            : base::PathService::Get(base::DIR_TEMP, &tmp_dir))) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // Change the current directory to the TMP directory. See method comment
  // for details.
  VLOG(1) << "Changing current directory to: " << tmp_dir.value();
  if (!base::SetCurrentDirectory(tmp_dir))
    PLOG(ERROR) << "Failed to change the current directory.";

  for (std::vector<base::FilePath>::const_iterator it = setup_files.begin();
       it != setup_files.end(); ++it) {
    const base::FilePath& setup_file = *it;
    if (!base::CreateTemporaryFileInDir(tmp_dir, &temp_file)) {
      LOG(ERROR) << "Failed to create temporary file for "
                 << setup_file.BaseName().value();
      return false;
    }

    VLOG(1) << "Attempting to move " << setup_file.BaseName().value()
            << " to: " << temp_file.value();
    if (!base::Move(setup_file, temp_file)) {
      PLOG(ERROR) << "Failed to move " << setup_file.BaseName().value()
                  << " to " << temp_file.value();
      return false;
    }

    // We cannot delete the file right away, but try to delete it some other
    // way. Either with the help of a different process or the system.
    if (!base::DeleteFileAfterReboot(temp_file)) {
      const uint32_t kDeleteAfterMs = 10 * 1000;
      DeleteFileFromTempProcess(temp_file, kDeleteAfterMs);
    }
  }
  return true;
}

}  // namespace installer
