// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the definitions of the installer functions that build
// the WorkItemList used to install the application.

#include "chrome/installer/setup/install_worker.h"

#include <atlsecurity.h>
#include <oaidl.h>
#include <sddl.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/install_service_work_item.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/update_active_setup_version_work_item.h"
#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/firewall_manager_win.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

using base::ASCIIToUTF16;
using base::win::RegKey;

namespace installer {

namespace {

constexpr wchar_t kChromeInstallFilesCapabilitySid[] =
    L"S-1-15-3-1024-3424233489-972189580-2057154623-747635277-1604371224-"
    L"316187997-3786583170-1043257646";
constexpr wchar_t kLpacChromeInstallFilesCapabilitySid[] =
    L"S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-"
    L"4240214049-4028510897-3317428798";

void AddInstallerCopyTasks(const InstallerState& installer_state,
                           const base::FilePath& setup_path,
                           const base::FilePath& archive_path,
                           const base::FilePath& temp_path,
                           const base::Version& new_version,
                           WorkItemList* install_list) {
  DCHECK(install_list);
  base::FilePath installer_dir(
      installer_state.GetInstallerDirectory(new_version));
  install_list->AddCreateDirWorkItem(installer_dir);

  base::FilePath exe_dst(installer_dir.Append(setup_path.BaseName()));

  if (exe_dst != setup_path) {
    install_list->AddCopyTreeWorkItem(setup_path.value(), exe_dst.value(),
                                      temp_path.value(), WorkItem::ALWAYS);
  }

  if (installer_state.RequiresActiveSetup()) {
    // Make a copy of setup.exe with a different name so that Active Setup
    // doesn't require an admin on XP thanks to Application Compatibility.
    base::FilePath active_setup_exe(installer_dir.Append(kActiveSetupExe));
    install_list->AddCopyTreeWorkItem(
        setup_path.value(), active_setup_exe.value(), temp_path.value(),
        WorkItem::ALWAYS);
  }

  base::FilePath archive_dst(installer_dir.Append(archive_path.BaseName()));
  if (archive_path != archive_dst) {
    // In the past, we copied rather than moved for system level installs so
    // that the permissions of %ProgramFiles% would be picked up.  Now that
    // |temp_path| is in %ProgramFiles% for system level installs (and in
    // %LOCALAPPDATA% otherwise), there is no need to do this for the archive.
    // Setup.exe, on the other hand, is created elsewhere so it must always be
    // copied.
    if (temp_path.IsParent(archive_path)) {
      install_list->AddMoveTreeWorkItem(archive_path.value(),
                                        archive_dst.value(),
                                        temp_path.value(),
                                        WorkItem::ALWAYS_MOVE);
    } else {
      // This may occur when setup is run out of an existing installation
      // directory. We cannot remove the system-level archive.
      install_list->AddCopyTreeWorkItem(archive_path.value(),
                                        archive_dst.value(),
                                        temp_path.value(),
                                        WorkItem::ALWAYS);
    }
  }
}

// A callback invoked by |work_item| that adds firewall rules for Chrome. Rules
// are left in-place on rollback unless |remove_on_rollback| is true. This is
// the case for new installs only. Updates and overinstalls leave the rule
// in-place on rollback since a previous install of Chrome will be used in that
// case.
bool AddFirewallRulesCallback(bool system_level,
                              const base::FilePath& chrome_path,
                              bool remove_on_rollback,
                              const CallbackWorkItem& work_item) {
  // There is no work to do on rollback if this is not a new install.
  if (work_item.IsRollback() && !remove_on_rollback)
    return true;

  std::unique_ptr<FirewallManager> manager =
      FirewallManager::Create(chrome_path);
  if (!manager) {
    LOG(ERROR) << "Failed creating a FirewallManager. Continuing with install.";
    return true;
  }

  if (work_item.IsRollback()) {
    manager->RemoveFirewallRules();
    return true;
  }

  // Adding the firewall rule is expected to fail for user-level installs on
  // Vista+. Try anyway in case the installer is running elevated.
  if (!manager->AddFirewallRules())
    LOG(ERROR) << "Failed creating a firewall rules. Continuing with install.";

  // Don't abort installation if the firewall rule couldn't be added.
  return true;
}

// Adds work items to |list| to create firewall rules.
void AddFirewallRulesWorkItems(const InstallerState& installer_state,
                               bool is_new_install,
                               WorkItemList* list) {
  list->AddCallbackWorkItem(
      base::Bind(&AddFirewallRulesCallback,
                 installer_state.system_install(),
                 installer_state.target_path().Append(kChromeExe),
                 is_new_install));
}

// Probes COM machinery to get an instance of notification_helper.exe's
// NotificationActivator class.
//
// This is required so that COM purges its cache of the path to the binary,
// which changes on updates.
//
// This callback unconditionally returns true since an install should not be
// aborted if the probe fails.
bool ProbeNotificationActivatorCallback(const CLSID& toast_activator_clsid,
                                        const CallbackWorkItem& work_item) {
  DCHECK(toast_activator_clsid != CLSID_NULL);

  // Noop on rollback.
  if (work_item.IsRollback())
    return true;

  Microsoft::WRL::ComPtr<IUnknown> notification_activator;
  HRESULT hr =
      ::CoCreateInstance(toast_activator_clsid, nullptr, CLSCTX_LOCAL_SERVER,
                         IID_PPV_ARGS(&notification_activator));

  if (hr != REGDB_E_CLASSNOTREG) {
    LOG(ERROR) << "Unexpected result creating NotificationActivator; hr=0x"
               << std::hex << hr;
  }

  return true;
}

// This is called when an MSI installation is run. It may be that a user is
// attempting to install the MSI on top of a non-MSI managed installation. If
// so, try and remove any existing "Add/Remove Programs" entry, as we want the
// uninstall to be managed entirely by the MSI machinery (accessible via the
// Add/Remove programs dialog).
void AddDeleteUninstallEntryForMSIWorkItems(
    const InstallerState& installer_state,
    WorkItemList* work_item_list) {
  DCHECK(installer_state.is_msi())
      << "This must only be called for MSI installations!";

  HKEY reg_root = installer_state.root_key();
  base::string16 uninstall_reg = install_static::GetUninstallRegistryPath();

  WorkItem* delete_reg_key = work_item_list->AddDeleteRegKeyWorkItem(
      reg_root, uninstall_reg, KEY_WOW64_32KEY);
  delete_reg_key->set_best_effort(true);
}

// Adds Chrome specific install work items to |install_list|.
// |current_version| can be NULL to indicate no Chrome is currently installed.
void AddChromeWorkItems(const InstallationState& original_state,
                        const InstallerState& installer_state,
                        const base::FilePath& setup_path,
                        const base::FilePath& archive_path,
                        const base::FilePath& src_path,
                        const base::FilePath& temp_path,
                        const base::Version* current_version,
                        const base::Version& new_version,
                        WorkItemList* install_list) {
  const base::FilePath& target_path = installer_state.target_path();

  if (current_version) {
    // Delete the archive from an existing install to save some disk space.
    base::FilePath old_installer_dir(
        installer_state.GetInstallerDirectory(*current_version));
    base::FilePath old_archive(
        old_installer_dir.Append(installer::kChromeArchive));
    // Don't delete the archive that we are actually installing from.
    if (archive_path != old_archive) {
      auto* delete_old_archive_work_item =
          install_list->AddDeleteTreeWorkItem(old_archive, temp_path);
      // Don't cause failure of |install_list| if this WorkItem fails.
      delete_old_archive_work_item->set_best_effort(true);
      // No need to roll this back; if installation fails we'll be moved to the
      // "-full" channel anyway.
      delete_old_archive_work_item->set_rollback_enabled(false);
    }
  }

  // Delete any new_chrome.exe if present (we will end up creating a new one
  // if required) and then copy chrome.exe
  base::FilePath new_chrome_exe(target_path.Append(installer::kChromeNewExe));

  install_list->AddDeleteTreeWorkItem(new_chrome_exe, temp_path);

  install_list->AddCopyTreeWorkItem(
      src_path.Append(installer::kChromeExe).value(),
      target_path.Append(installer::kChromeExe).value(), temp_path.value(),
      WorkItem::NEW_NAME_IF_IN_USE, new_chrome_exe.value());

  // Install kVisualElementsManifest if it is present in |src_path|. No need to
  // make this a conditional work item as if the file is not there now, it will
  // never be.
  // TODO(grt): Touch the Start Menu shortcut after putting the manifest in
  // place to force the Start Menu to refresh Chrome's tile.
  if (base::PathExists(
          src_path.Append(installer::kVisualElementsManifest))) {
    install_list->AddMoveTreeWorkItem(
        src_path.Append(installer::kVisualElementsManifest).value(),
        target_path.Append(installer::kVisualElementsManifest).value(),
        temp_path.value(),
        WorkItem::ALWAYS_MOVE);
  } else {
    // We do not want to have an old VisualElementsManifest pointing to an old
    // version directory. Delete it as there wasn't a new one to replace it.
    install_list->AddDeleteTreeWorkItem(
        target_path.Append(installer::kVisualElementsManifest),
        temp_path);
  }

  // In the past, we copied rather than moved for system level installs so that
  // the permissions of %ProgramFiles% would be picked up.  Now that |temp_path|
  // is in %ProgramFiles% for system level installs (and in %LOCALAPPDATA%
  // otherwise), there is no need to do this.
  // Note that we pass true for check_duplicates to avoid failing on in-use
  // repair runs if the current_version is the same as the new_version.
  bool check_for_duplicates = (current_version &&
                               *current_version == new_version);
  install_list->AddMoveTreeWorkItem(
      src_path.AppendASCII(new_version.GetString()).value(),
      target_path.AppendASCII(new_version.GetString()).value(),
      temp_path.value(),
      check_for_duplicates ? WorkItem::CHECK_DUPLICATES :
                             WorkItem::ALWAYS_MOVE);

  // Delete any old_chrome.exe if present (ignore failure if it's in use).
  install_list
      ->AddDeleteTreeWorkItem(target_path.Append(installer::kChromeOldExe),
                              temp_path)
      ->set_best_effort(true);
}

// Adds an ACE from a trustee SID, access mask and flags to an existing DACL.
// If the exact ACE already exists then the DACL is not modified and true is
// returned.
bool AddAceToDacl(const ATL::CSid& trustee,
                  ACCESS_MASK access_mask,
                  BYTE ace_flags,
                  ATL::CDacl* dacl) {
  // Check if the requested access already exists and return if so.
  for (UINT i = 0; i < dacl->GetAceCount(); ++i) {
    ATL::CSid sid;
    ACCESS_MASK mask = 0;
    BYTE type = 0;
    BYTE flags = 0;
    dacl->GetAclEntry(i, &sid, &mask, &type, &flags);
    if (sid == trustee && type == ACCESS_ALLOWED_ACE_TYPE &&
        (flags & ace_flags) == ace_flags &&
        (mask & access_mask) == access_mask) {
      return true;
    }
  }

  // Add the new access to the DACL.
  return dacl->AddAllowedAce(trustee, access_mask, ace_flags);
}

// Add to the ACL of an object on disk. This follows the method from MSDN:
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa379283.aspx
// This is done using explicit flags rather than the "security string" format
// because strings do not necessarily read what is written which makes it
// difficult to de-dup. Working with the binary format is always exact and the
// system libraries will properly ignore duplicate ACL entries.
bool AddAclToPath(const base::FilePath& path,
                  const std::vector<ATL::CSid>& trustees,
                  ACCESS_MASK access_mask,
                  BYTE ace_flags) {
  DCHECK(!path.empty());

  // Get the existing DACL.
  ATL::CDacl dacl;
  if (!ATL::AtlGetDacl(path.value().c_str(), SE_FILE_OBJECT, &dacl)) {
    DPLOG(ERROR) << "Failed getting DACL for path \"" << path.value() << "\"";
    return false;
  }

  for (const auto& trustee : trustees) {
    DCHECK(trustee.IsValid());
    if (!AddAceToDacl(trustee, access_mask, ace_flags, &dacl)) {
      DPLOG(ERROR) << "Failed adding ACE to DACL for trustee " << trustee.Sid();
      return false;
    }
  }

  // Attach the updated ACL as the object's DACL.
  if (!ATL::AtlSetDacl(path.value().c_str(), SE_FILE_OBJECT, dacl)) {
    DPLOG(ERROR) << "Failed setting DACL for path \"" << path.value() << "\"";
    return false;
  }

  return true;
}

bool AddAclToPath(const base::FilePath& path,
                  const CSid& trustee,
                  ACCESS_MASK access_mask,
                  BYTE ace_flags) {
  std::vector<ATL::CSid> trustees = {trustee};
  return AddAclToPath(path, trustees, access_mask, ace_flags);
}

bool AddAclToPath(const base::FilePath& path,
                  const std::vector<const wchar_t*>& trustees,
                  ACCESS_MASK access_mask,
                  BYTE ace_flags) {
  std::vector<ATL::CSid> converted_trustees;
  for (const wchar_t* trustee : trustees) {
    PSID sid;
    if (!::ConvertStringSidToSid(trustee, &sid)) {
      DPLOG(ERROR) << "Failed to convert SID \"" << trustee << "\"";
      return false;
    }
    converted_trustees.emplace_back(static_cast<SID*>(sid));
    ::LocalFree(sid);
  }

  return AddAclToPath(path, converted_trustees, access_mask, ace_flags);
}

// Migrates consent for the collection of usage statistics from the binaries to
// Chrome when migrating multi-install Chrome to single-install.
void AddMigrateUsageStatsWorkItems(const InstallerState& installer_state,
                                   WorkItemList* install_list) {
  // This operation only applies to modes that once supported multi-install.
  if (install_static::InstallDetails::Get().supported_multi_install())
    return;

  // Bail out if an existing multi-install Chrome is not being migrated to
  // single-install.
  if (!installer_state.is_migrating_to_single())
    return;

  // Nothing to do if the binaries aren't actually installed.
  if (!AreBinariesInstalled(installer_state))
    return;

  // Delete any stale value in Chrome's ClientStateMedium key. A new value, if
  // found, will be written to the ClientState key below.
  if (installer_state.system_install()) {
    install_list->AddDeleteRegValueWorkItem(
        installer_state.root_key(),
        install_static::GetClientStateMediumKeyPath(), KEY_WOW64_64KEY,
        google_update::kRegUsageStatsField);
  }

  google_update::Tristate consent =
      GoogleUpdateSettings::GetCollectStatsConsentForBinaries();
  if (consent == google_update::TRISTATE_NONE) {
    VLOG(1) << "No consent value found to migrate to single-install.";
    // Delete any stale value in Chrome's ClientState key.
    install_list->AddDeleteRegValueWorkItem(
        installer_state.root_key(), install_static::GetClientStateKeyPath(),
        KEY_WOW64_64KEY, google_update::kRegUsageStatsField);
    return;
  }

  VLOG(1) << "Migrating usage stats consent from multi- to single-install.";

  // Write consent to Chrome's ClientState key.
  install_list->AddSetRegValueWorkItem(
      installer_state.root_key(), install_static::GetClientStateKeyPath(),
      KEY_WOW64_32KEY, google_update::kRegUsageStatsField,
      static_cast<DWORD>(consent), true);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Adds work items to register the Elevation Service with Windows. Only for
// system level installs.
void AddElevationServiceWorkItems(const base::FilePath& elevation_service_path,
                                  WorkItemList* list) {
  DCHECK(::IsUserAnAdmin());
  const HKEY root = HKEY_LOCAL_MACHINE;

  if (elevation_service_path.empty()) {
    LOG(DFATAL) << "The path to elevation_service.exe is invalid.";
    return;
  }

  const base::string16 clsid_reg_path = GetElevationServiceClsidRegistryPath();
  const base::string16 appid_reg_path = GetElevationServiceAppidRegistryPath();
  const base::string16 iid_reg_path = GetElevationServiceIidRegistryPath();
  const base::string16 typelib_reg_path =
      GetElevationServiceTypeLibRegistryPath();

  // Delete any old registrations first, taking into account 32-bit -> 64-bit or
  // 64-bit -> 32-bit migration.
  for (const auto& reg_path :
       {clsid_reg_path, appid_reg_path, iid_reg_path, typelib_reg_path}) {
    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
      list->AddDeleteRegKeyWorkItem(root, reg_path, key_flag);
  }

  list->AddWorkItem(new InstallServiceWorkItem(
      install_static::GetElevationServiceName(),
      install_static::GetElevationServiceDisplayName(),
      base::CommandLine(elevation_service_path)));

  list->AddCreateRegKeyWorkItem(root, clsid_reg_path, WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, clsid_reg_path, WorkItem::kWow64Default,
                               L"AppID", GetElevationServiceGuid(L""), true);
  list->AddCreateRegKeyWorkItem(root, appid_reg_path, WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, appid_reg_path, WorkItem::kWow64Default,
                               L"LocalService",
                               install_static::GetElevationServiceName(), true);

  // Registering the Ole Automation marshaler with the CLSID
  // {00020424-0000-0000-C000-000000000046} as the proxy/stub for the IElevator
  // interface.
  list->AddCreateRegKeyWorkItem(root, iid_reg_path, WorkItem::kWow64Default);
  list->AddCreateRegKeyWorkItem(root, iid_reg_path + L"\\ProxyStubClsid32",
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, iid_reg_path + L"\\ProxyStubClsid32",
                               WorkItem::kWow64Default, L"",
                               L"{00020424-0000-0000-C000-000000000046}", true);
  list->AddCreateRegKeyWorkItem(root, iid_reg_path + L"\\TypeLib",
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, iid_reg_path + L"\\TypeLib",
                               WorkItem::kWow64Default, L"",
                               GetElevationServiceIid(L""), true);

  // The TypeLib registration for the Ole Automation marshaler.
  list->AddCreateRegKeyWorkItem(root, typelib_reg_path,
                                WorkItem::kWow64Default);
  list->AddCreateRegKeyWorkItem(root, typelib_reg_path + L"\\1.0",
                                WorkItem::kWow64Default);
  list->AddCreateRegKeyWorkItem(root, typelib_reg_path + L"\\1.0\\0",
                                WorkItem::kWow64Default);
  list->AddCreateRegKeyWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win32",
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win32",
                               WorkItem::kWow64Default, L"",
                               elevation_service_path.value(), true);
  list->AddCreateRegKeyWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win64",
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win64",
                               WorkItem::kWow64Default, L"",
                               elevation_service_path.value(), true);
}

// Adds work items to add or remove the "store-dmtoken" command to Chrome's
// version key. This method is a no-op if this is anything other than
// system-level Chrome. The command is used when enrolling Chrome browser
// instances into enterprise management. |new_version| is the version currently
// being installed -- can be empty on uninstall.
void AddEnterpriseEnrollmentWorkItems(const InstallerState& installer_state,
                                      const base::FilePath& setup_path,
                                      const base::Version& new_version,
                                      WorkItemList* install_list) {
  if (!installer_state.system_install())
    return;

  const HKEY root_key = installer_state.root_key();
  const base::string16 cmd_key(GetCommandKey(kCmdStoreDMToken));

  if (installer_state.operation() == InstallerState::UNINSTALL) {
    install_list->AddDeleteRegKeyWorkItem(root_key, cmd_key, KEY_WOW64_32KEY)
        ->set_log_message("Removing store DM token command");
  } else {
    // Register a command to allow Chrome to request Google Update to run
    // setup.exe --store-dmtoken=<token>, which will store the specified token
    // in the registry.
    base::CommandLine cmd_line(
        installer_state.GetInstallerDirectory(new_version)
            .Append(setup_path.BaseName()));
    cmd_line.AppendSwitchASCII(switches::kStoreDMToken, "%1");
    cmd_line.AppendSwitch(switches::kSystemLevel);
    cmd_line.AppendSwitch(switches::kVerboseLogging);
    InstallUtil::AppendModeSwitch(&cmd_line);

    AppCommand cmd(cmd_line.GetCommandLineString());
    // TODO(alito): For now setting this command as web accessible is required
    // by Google Update.  Could revisit this should Google Update change the
    // way permissions are handled for commands.
    cmd.set_is_web_accessible(true);
    cmd.AddWorkItems(root_key, cmd_key, install_list);
  }
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

// This method adds work items to create (or update) Chrome uninstall entry in
// either the Control Panel->Add/Remove Programs list or in the Omaha client
// state key if running under an MSI installer.
void AddUninstallShortcutWorkItems(const InstallerState& installer_state,
                                   const base::FilePath& setup_path,
                                   const base::Version& new_version,
                                   WorkItemList* install_list) {
  HKEY reg_root = installer_state.root_key();

  // When we are installed via an MSI, we need to store our uninstall strings
  // in the Google Update client state key. We do this even for non-MSI
  // managed installs to avoid breaking the edge case whereby an MSI-managed
  // install is updated by a non-msi installer (which would confuse the MSI
  // machinery if these strings were not also updated). The UninstallString
  // value placed in the client state key is also used by the mini_installer to
  // locate the setup.exe instance used for binary patching.
  // Do not quote the command line for the MSI invocation.
  base::FilePath install_path(installer_state.target_path());
  base::FilePath installer_path(
      installer_state.GetInstallerDirectory(new_version));
  installer_path = installer_path.Append(setup_path.BaseName());

  base::CommandLine uninstall_arguments(base::CommandLine::NO_PROGRAM);
  AppendUninstallCommandLineFlags(installer_state, &uninstall_arguments);

  base::string16 update_state_key(install_static::GetClientStateKeyPath());
  install_list->AddCreateRegKeyWorkItem(
      reg_root, update_state_key, KEY_WOW64_32KEY);
  install_list->AddSetRegValueWorkItem(reg_root,
                                       update_state_key,
                                       KEY_WOW64_32KEY,
                                       installer::kUninstallStringField,
                                       installer_path.value(),
                                       true);
  install_list->AddSetRegValueWorkItem(
      reg_root,
      update_state_key,
      KEY_WOW64_32KEY,
      installer::kUninstallArgumentsField,
      uninstall_arguments.GetCommandLineString(),
      true);

  // MSI installations will manage their own uninstall shortcuts.
  if (!installer_state.is_msi()) {
    // We need to quote the command line for the Add/Remove Programs dialog.
    base::CommandLine quoted_uninstall_cmd(installer_path);
    DCHECK_EQ(quoted_uninstall_cmd.GetCommandLineString()[0], '"');
    quoted_uninstall_cmd.AppendArguments(uninstall_arguments, false);

    base::string16 uninstall_reg = install_static::GetUninstallRegistryPath();
    install_list->AddCreateRegKeyWorkItem(
        reg_root, uninstall_reg, KEY_WOW64_32KEY);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         installer::kUninstallDisplayNameField,
                                         InstallUtil::GetDisplayName(), true);
    install_list->AddSetRegValueWorkItem(
        reg_root,
        uninstall_reg,
        KEY_WOW64_32KEY,
        installer::kUninstallStringField,
        quoted_uninstall_cmd.GetCommandLineString(),
        true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"InstallLocation",
                                         install_path.value(),
                                         true);

    base::string16 chrome_icon =
        ShellUtil::FormatIconLocation(install_path.Append(kChromeExe),
                                      install_static::GetIconResourceIndex());
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"DisplayIcon",
                                         chrome_icon,
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"NoModify",
                                         static_cast<DWORD>(1),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"NoRepair",
                                         static_cast<DWORD>(1),
                                         true);

    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY, L"Publisher",
                                         InstallUtil::GetPublisherName(), true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"Version",
                                         ASCIIToUTF16(new_version.GetString()),
                                         true);
    install_list->AddSetRegValueWorkItem(reg_root,
                                         uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         L"DisplayVersion",
                                         ASCIIToUTF16(new_version.GetString()),
                                         true);
    // TODO(wfh): Ensure that this value is preserved in the 64-bit hive when
    // 64-bit installs place the uninstall information into the 64-bit registry.
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY, L"InstallDate",
                                         InstallUtil::GetCurrentDate(), true);

    const std::vector<uint32_t>& version_components = new_version.components();
    if (version_components.size() == 4) {
      // Our version should be in major.minor.build.rev.
      install_list->AddSetRegValueWorkItem(
          reg_root,
          uninstall_reg,
          KEY_WOW64_32KEY,
          L"VersionMajor",
          static_cast<DWORD>(version_components[2]),
          true);
      install_list->AddSetRegValueWorkItem(
          reg_root,
          uninstall_reg,
          KEY_WOW64_32KEY,
          L"VersionMinor",
          static_cast<DWORD>(version_components[3]),
          true);
    }
  }
}

// Create Version key for a product (if not already present) and sets the new
// product version as the last step.
void AddVersionKeyWorkItems(HKEY root,
                            const base::Version& new_version,
                            bool add_language_identifier,
                            WorkItemList* list) {
  const base::string16 clients_key = install_static::GetClientsKeyPath();
  list->AddCreateRegKeyWorkItem(root, clients_key, KEY_WOW64_32KEY);

  list->AddSetRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                               google_update::kRegNameField,
                               InstallUtil::GetDisplayName(),
                               true);  // overwrite name also
  list->AddSetRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                               google_update::kRegOopcrashesField,
                               static_cast<DWORD>(1),
                               false);  // set during first install
  if (add_language_identifier) {
    // Write the language identifier of the current translation.  Omaha's set of
    // languages is a superset of Chrome's set of translations with this one
    // exception: what Chrome calls "en-us", Omaha calls "en".  sigh.
    base::string16 language(GetCurrentTranslation());
    if (base::LowerCaseEqualsASCII(language, "en-us"))
      language.resize(2);
    list->AddSetRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                                 google_update::kRegLangField, language,
                                 false);  // do not overwrite language
  }
  list->AddSetRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                               google_update::kRegVersionField,
                               ASCIIToUTF16(new_version.GetString()),
                               true);  // overwrite version
}

void AddUpdateBrandCodeWorkItem(const InstallerState& installer_state,
                                WorkItemList* install_list) {
  // Only update specific brand codes needed for enterprise.
  base::string16 brand;
  if (!GoogleUpdateSettings::GetBrand(&brand))
    return;

  base::string16 new_brand = GetUpdatedBrandCode(brand);
  if (new_brand.empty())
    return;

  // Only update if this machine is:
  // - domain joined, or
  // - registered with MDM and is not windows home edition
  bool is_enterprise_version =
      base::win::OSInfo::GetInstance()->version_type() != base::win::SUITE_HOME;
  if (!(base::win::IsEnrolledToDomain() ||
      (base::win::IsDeviceRegisteredWithManagement() &&
       is_enterprise_version))) {
    return;
  }

  install_list->AddSetRegValueWorkItem(
      installer_state.root_key(), install_static::GetClientStateKeyPath(),
      KEY_WOW64_32KEY, google_update::kRegRLZBrandField, new_brand, true);
}

base::string16 GetUpdatedBrandCode(const base::string16& brand_code) {
  // Brand codes to be remapped on enterprise installs.
  static constexpr struct EnterpriseBrandRemapping {
    const wchar_t* old_brand;
    const wchar_t* new_brand;
  } kEnterpriseBrandRemapping[] = {
      {L"GGLS", L"GCEU"},
      {L"GGRV", L"GCEV"},
  };

  for (auto mapping : kEnterpriseBrandRemapping) {
    if (brand_code == mapping.old_brand)
      return mapping.new_brand;
  }
  return base::string16();
}

bool AppendPostInstallTasks(const InstallerState& installer_state,
                            const base::FilePath& setup_path,
                            const base::FilePath& src_path,
                            const base::FilePath& temp_path,
                            const base::Version* current_version,
                            const base::Version& new_version,
                            WorkItemList* post_install_task_list) {
  DCHECK(post_install_task_list);

  HKEY root = installer_state.root_key();
  const base::FilePath& target_path = installer_state.target_path();
  base::FilePath new_chrome_exe(target_path.Append(kChromeNewExe));

  // Append work items that will only be executed if this was an update.
  // We update the 'opv' value with the current version that is active,
  // the 'cpv' value with the critical update version (if present), and the
  // 'cmd' value with the rename command to run.
  {
    std::unique_ptr<WorkItemList> in_use_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new ConditionRunIfFileExists(new_chrome_exe)));
    in_use_update_work_items->set_log_message("InUseUpdateWorkItemList");

    // |critical_version| will be valid only if this in-use update includes a
    // version considered critical relative to the version being updated.
    base::Version critical_version(installer_state.DetermineCriticalVersion(
        current_version, new_version));
    base::FilePath installer_path(
        installer_state.GetInstallerDirectory(new_version).Append(
            setup_path.BaseName()));

    const base::string16 clients_key(install_static::GetClientsKeyPath());

    if (current_version) {
      in_use_update_work_items->AddSetRegValueWorkItem(
          root, clients_key, KEY_WOW64_32KEY,
          google_update::kRegOldVersionField,
          ASCIIToUTF16(current_version->GetString()), true);
    }
    if (critical_version.IsValid()) {
      in_use_update_work_items->AddSetRegValueWorkItem(
          root, clients_key, KEY_WOW64_32KEY,
          google_update::kRegCriticalVersionField,
          ASCIIToUTF16(critical_version.GetString()), true);
    } else {
      in_use_update_work_items->AddDeleteRegValueWorkItem(
          root, clients_key, KEY_WOW64_32KEY,
          google_update::kRegCriticalVersionField);
    }

    // Form the mode-specific rename command.
    base::CommandLine product_rename_cmd(installer_path);
    product_rename_cmd.AppendSwitch(switches::kRenameChromeExe);
    if (installer_state.system_install())
      product_rename_cmd.AppendSwitch(switches::kSystemLevel);
    if (installer_state.verbose_logging())
      product_rename_cmd.AppendSwitch(switches::kVerboseLogging);
    InstallUtil::AppendModeSwitch(&product_rename_cmd);
    in_use_update_work_items->AddSetRegValueWorkItem(
        root, clients_key, KEY_WOW64_32KEY, google_update::kRegRenameCmdField,
        product_rename_cmd.GetCommandLineString(), true);

    // Delay deploying the new chrome_proxy while chrome is running.
    in_use_update_work_items->AddCopyTreeWorkItem(
        src_path.Append(kChromeProxyExe).value(),
        target_path.Append(kChromeProxyNewExe).value(), temp_path.value(),
        WorkItem::ALWAYS);

    post_install_task_list->AddWorkItem(in_use_update_work_items.release());
  }

  // Append work items that will be executed if this was NOT an in-use update.
  {
    std::unique_ptr<WorkItemList> regular_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new Not(new ConditionRunIfFileExists(new_chrome_exe))));
    regular_update_work_items->set_log_message("RegularUpdateWorkItemList");

    // Since this was not an in-use-update, delete 'opv', 'cpv', and 'cmd' keys.
    const base::string16 clients_key(install_static::GetClientsKeyPath());
    regular_update_work_items->AddDeleteRegValueWorkItem(
        root, clients_key, KEY_WOW64_32KEY, google_update::kRegOldVersionField);
    regular_update_work_items->AddDeleteRegValueWorkItem(
        root, clients_key, KEY_WOW64_32KEY,
        google_update::kRegCriticalVersionField);
    regular_update_work_items->AddDeleteRegValueWorkItem(
        root, clients_key, KEY_WOW64_32KEY, google_update::kRegRenameCmdField);

    // Only copy chrome_proxy.exe directly when chrome.exe isn't in use to avoid
    // different versions getting mixed up between the two binaries.
    regular_update_work_items->AddCopyTreeWorkItem(
        src_path.Append(kChromeProxyExe).value(),
        target_path.Append(kChromeProxyExe).value(), temp_path.value(),
        WorkItem::ALWAYS);

    post_install_task_list->AddWorkItem(regular_update_work_items.release());
  }

  // If we're told that we're an MSI install, make sure to set the marker
  // in the client state key so that future updates do the right thing.
  if (installer_state.is_msi()) {
    AddSetMsiMarkerWorkItem(installer_state, true, post_install_task_list);

    // We want MSI installs to take over the Add/Remove Programs entry. Make a
    // best-effort attempt to delete any entry left over from previous non-MSI
    // installations for the same type of install (system or per user).
    AddDeleteUninstallEntryForMSIWorkItems(installer_state,
                                           post_install_task_list);
  }

  // Add a best-effort item to create the ClientStateMedium key for system-level
  // installs. This is ordinarily done by Google Update prior to running
  // Chrome's installer. Do it here as well so that the key exists for manual
  // installs.
  if (install_static::kUseGoogleUpdateIntegration &&
      installer_state.system_install()) {
    const base::string16 path = install_static::GetClientStateMediumKeyPath();
    post_install_task_list
        ->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, path, KEY_WOW64_32KEY)
        ->set_best_effort(true);
  }

  return true;
}

void AddInstallWorkItems(const InstallationState& original_state,
                         const InstallerState& installer_state,
                         const base::FilePath& setup_path,
                         const base::FilePath& archive_path,
                         const base::FilePath& src_path,
                         const base::FilePath& temp_path,
                         const base::Version* current_version,
                         const base::Version& new_version,
                         WorkItemList* install_list) {
  DCHECK(install_list);

  const base::FilePath& target_path = installer_state.target_path();

  // A temp directory that work items need and the actual install directory.
  install_list->AddCreateDirWorkItem(temp_path);
  install_list->AddCreateDirWorkItem(target_path);

  // Set permissions early on both temp and target, since moved files may not
  // inherit permissions.
  WorkItem* add_ac_acl_to_install =
      install_list->AddCallbackWorkItem(base::BindRepeating(
          [](const base::FilePath& target_path, const base::FilePath& temp_path,
             const CallbackWorkItem& work_item) {
            DCHECK(!work_item.IsRollback());
            std::vector<const wchar_t*> sids = {
                kChromeInstallFilesCapabilitySid,
                kLpacChromeInstallFilesCapabilitySid};
            bool success_target = AddAclToPath(
                target_path, sids, FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
                CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
            bool success_temp = AddAclToPath(
                temp_path, sids, FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
                CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);

            bool success = (success_target && success_temp);
            base::UmaHistogramBoolean("Setup.Install.AddAppContainerAce",
                                      success);
            return success;
          },
          target_path, temp_path));
  add_ac_acl_to_install->set_best_effort(true);
  add_ac_acl_to_install->set_rollback_enabled(false);

  // Create the directory in which persistent metrics will be stored.
  const base::FilePath histogram_storage_dir(
      target_path.AppendASCII(kSetupHistogramAllocatorName));
  install_list->AddCreateDirWorkItem(histogram_storage_dir);

  if (installer_state.system_install()) {
    WorkItem* add_acl_to_histogram_storage_dir_work_item =
        install_list->AddCallbackWorkItem(base::Bind(
            [](const base::FilePath& histogram_storage_dir,
               const CallbackWorkItem& work_item) {
              DCHECK(!work_item.IsRollback());
              return AddAclToPath(histogram_storage_dir,
                                  ATL::Sids::AuthenticatedUser(),
                                  FILE_GENERIC_READ | FILE_DELETE_CHILD,
                                  CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
            },
            histogram_storage_dir));
    add_acl_to_histogram_storage_dir_work_item->set_best_effort(true);
    add_acl_to_histogram_storage_dir_work_item->set_rollback_enabled(false);
  }

  AddChromeWorkItems(original_state, installer_state, setup_path, archive_path,
                     src_path, temp_path, current_version, new_version,
                     install_list);

  // Copy installer in install directory
  AddInstallerCopyTasks(installer_state, setup_path, archive_path, temp_path,
                        new_version, install_list);

  const HKEY root = installer_state.root_key();
  // Only set "lang" for user-level installs since for system-level, the install
  // language may not be related to a given user's runtime language.
  const bool add_language_identifier = !installer_state.system_install();

  AddUninstallShortcutWorkItems(installer_state, setup_path, new_version,
                                install_list);

  AddVersionKeyWorkItems(root, new_version, add_language_identifier,
                         install_list);

  AddCleanupDeprecatedPerUserRegistrationsWorkItems(install_list);

  AddActiveSetupWorkItems(installer_state, new_version, install_list);

  AddOsUpgradeWorkItems(installer_state, setup_path, new_version, install_list);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddEnterpriseEnrollmentWorkItems(installer_state, setup_path, new_version,
                                   install_list);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING
  AddFirewallRulesWorkItems(installer_state, current_version == nullptr,
                            install_list);

  // We don't have a version check for Win10+ here so that Windows upgrades
  // work.
  AddNativeNotificationWorkItems(
      installer_state.root_key(),
      GetNotificationHelperPath(target_path, new_version), install_list);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (installer_state.system_install()) {
    AddElevationServiceWorkItems(
        GetElevationServicePath(target_path, new_version), install_list);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING

  InstallUtil::AddUpdateDowngradeVersionItem(
      installer_state.root_key(), current_version, new_version, install_list);

  // Migrate usagestats back to Chrome.
  AddMigrateUsageStatsWorkItems(installer_state, install_list);

  AddUpdateBrandCodeWorkItem(installer_state, install_list);

  // Append the tasks that run after the installation.
  AppendPostInstallTasks(installer_state, setup_path, src_path, temp_path,
                         current_version, new_version, install_list);
}

void AddNativeNotificationWorkItems(
    HKEY root,
    const base::FilePath& notification_helper_path,
    WorkItemList* list) {
  if (notification_helper_path.empty()) {
    LOG(DFATAL) << "The path to notification_helper.exe is invalid.";
    return;
  }

  base::string16 toast_activator_reg_path =
      InstallUtil::GetToastActivatorRegistryPath();

  if (toast_activator_reg_path.empty()) {
    LOG(DFATAL) << "Cannot retrieve the toast activator registry path";
    return;
  }

  // Delete the old registration before adding in the new key to ensure that the
  // COM probe/flush below does its job. Delete both 64-bit and 32-bit keys to
  // handle 32-bit -> 64-bit or 64-bit -> 32-bit migration.
  list->AddDeleteRegKeyWorkItem(root, toast_activator_reg_path,
                                KEY_WOW64_32KEY);

  list->AddDeleteRegKeyWorkItem(root, toast_activator_reg_path,
                                KEY_WOW64_64KEY);

  // Force COM to flush its cache containing the path to the old handler.
  list->AddCallbackWorkItem(
      base::BindRepeating(&ProbeNotificationActivatorCallback,
                          install_static::GetToastActivatorClsid()));

  base::string16 toast_activator_server_path =
      toast_activator_reg_path + L"\\LocalServer32";

  // Command-line featuring the quoted path to the exe.
  base::string16 command(1, L'"');
  command.append(notification_helper_path.value()).append(1, L'"');

  list->AddCreateRegKeyWorkItem(root, toast_activator_server_path,
                                WorkItem::kWow64Default);

  list->AddSetRegValueWorkItem(root, toast_activator_server_path,
                               WorkItem::kWow64Default, L"", command, true);

  list->AddSetRegValueWorkItem(root, toast_activator_server_path,
                               WorkItem::kWow64Default, L"ServerExecutable",
                               notification_helper_path.value(), true);
}

void AddSetMsiMarkerWorkItem(const InstallerState& installer_state,
                             bool set,
                             WorkItemList* work_item_list) {
  DCHECK(work_item_list);
  DWORD msi_value = set ? 1 : 0;
  WorkItem* set_msi_work_item = work_item_list->AddSetRegValueWorkItem(
      installer_state.root_key(), install_static::GetClientStateKeyPath(),
      KEY_WOW64_32KEY, google_update::kRegMSIField, msi_value, true);
  DCHECK(set_msi_work_item);
  set_msi_work_item->set_best_effort(true);
  set_msi_work_item->set_log_message("Could not write MSI marker!");
}

void AddCleanupDeprecatedPerUserRegistrationsWorkItems(WorkItemList* list) {
  // This cleanup was added in M49. There are still enough active users on M48
  // and earlier today (M55 timeframe) to justify keeping this cleanup in-place.
  // Remove this when that population stops shrinking.
  VLOG(1) << "Adding unregistration items for per-user Metro keys.";
  list->AddDeleteRegKeyWorkItem(HKEY_CURRENT_USER,
                                install_static::GetRegistryPath() + L"\\Metro",
                                KEY_WOW64_32KEY);
  list->AddDeleteRegKeyWorkItem(HKEY_CURRENT_USER,
                                install_static::GetRegistryPath() + L"\\Metro",
                                KEY_WOW64_64KEY);
}

void AddActiveSetupWorkItems(const InstallerState& installer_state,
                             const base::Version& new_version,
                             WorkItemList* list) {
  DCHECK(installer_state.operation() != InstallerState::UNINSTALL);

  if (!installer_state.system_install()) {
    VLOG(1) << "No Active Setup processing to do for user-level Chrome";
    return;
  }
  DCHECK(installer_state.RequiresActiveSetup());

  const HKEY root = HKEY_LOCAL_MACHINE;
  const base::string16 active_setup_path(install_static::GetActiveSetupPath());

  VLOG(1) << "Adding registration items for Active Setup.";
  list->AddCreateRegKeyWorkItem(
      root, active_setup_path, WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, active_setup_path, WorkItem::kWow64Default,
                               L"", InstallUtil::GetDisplayName(), true);

  base::FilePath active_setup_exe(installer_state.GetInstallerDirectory(
      new_version).Append(kActiveSetupExe));
  base::CommandLine cmd(active_setup_exe);
  cmd.AppendSwitch(installer::switches::kConfigureUserSettings);
  cmd.AppendSwitch(installer::switches::kVerboseLogging);
  cmd.AppendSwitch(installer::switches::kSystemLevel);
  InstallUtil::AppendModeSwitch(&cmd);
  list->AddSetRegValueWorkItem(root,
                               active_setup_path,
                               WorkItem::kWow64Default,
                               L"StubPath",
                               cmd.GetCommandLineString(),
                               true);

  // TODO(grt): http://crbug.com/75152 Write a reference to a localized
  // resource.
  list->AddSetRegValueWorkItem(root, active_setup_path, WorkItem::kWow64Default,
                               L"Localized Name", InstallUtil::GetDisplayName(),
                               true);

  list->AddSetRegValueWorkItem(root,
                               active_setup_path,
                               WorkItem::kWow64Default,
                               L"IsInstalled",
                               static_cast<DWORD>(1U),
                               true);

  list->AddWorkItem(new UpdateActiveSetupVersionWorkItem(
      active_setup_path, UpdateActiveSetupVersionWorkItem::UPDATE));
}

void AppendUninstallCommandLineFlags(const InstallerState& installer_state,
                                     base::CommandLine* uninstall_cmd) {
  DCHECK(uninstall_cmd);

  uninstall_cmd->AppendSwitch(installer::switches::kUninstall);

  InstallUtil::AppendModeSwitch(uninstall_cmd);
  if (installer_state.is_msi())
    uninstall_cmd->AppendSwitch(installer::switches::kMsi);
  if (installer_state.system_install())
    uninstall_cmd->AppendSwitch(installer::switches::kSystemLevel);
  if (installer_state.verbose_logging())
    uninstall_cmd->AppendSwitch(installer::switches::kVerboseLogging);
}

void AddOsUpgradeWorkItems(const InstallerState& installer_state,
                           const base::FilePath& setup_path,
                           const base::Version& new_version,
                           WorkItemList* install_list) {
  const HKEY root_key = installer_state.root_key();
  const base::string16 cmd_key(GetCommandKey(kCmdOnOsUpgrade));

  if (installer_state.operation() == InstallerState::UNINSTALL) {
    install_list->AddDeleteRegKeyWorkItem(root_key, cmd_key, KEY_WOW64_32KEY)
        ->set_log_message("Removing OS upgrade command");
  } else {
    // Register with Google Update to have setup.exe --on-os-upgrade called on
    // OS upgrade.
    base::CommandLine cmd_line(
        installer_state.GetInstallerDirectory(new_version)
            .Append(setup_path.BaseName()));
    // Add the main option to indicate OS upgrade flow.
    cmd_line.AppendSwitch(installer::switches::kOnOsUpgrade);
    InstallUtil::AppendModeSwitch(&cmd_line);
    if (installer_state.system_install())
      cmd_line.AppendSwitch(installer::switches::kSystemLevel);
    // Log everything for now.
    cmd_line.AppendSwitch(installer::switches::kVerboseLogging);

    AppCommand cmd(cmd_line.GetCommandLineString());
    cmd.set_is_auto_run_on_os_upgrade(true);
    cmd.AddWorkItems(installer_state.root_key(), cmd_key, install_list);
  }
}

}  // namespace installer
