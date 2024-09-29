// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the definitions of the installer functions that build
// the WorkItemList used to install the application.

#include "chrome/installer/setup/install_worker.h"

#include <windows.h>

#include <oaidl.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/configure_app_container_sandbox.h"
#include "chrome/installer/setup/downgrade_cleanup.h"
#include "chrome/installer/setup/install_params.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/last_breaking_installer_version.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/update_active_setup_version_work_item.h"
#include "chrome/installer/util/app_command.h"
#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/firewall_manager_win.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
#include "chrome/installer/setup/channel_override_work_item.h"
#endif

using base::ASCIIToWide;
using base::win::RegKey;

namespace installer {

namespace {

void AddInstallerCopyTasks(const InstallParams& install_params,
                           WorkItemList* install_list) {
  DCHECK(install_list);

  const InstallerState& installer_state = *install_params.installer_state;
  const base::FilePath& setup_path = *install_params.setup_path;
  const base::FilePath& archive_path = *install_params.archive_path;
  const base::FilePath& temp_path = *install_params.temp_path;
  const base::Version& new_version = *install_params.new_version;

  base::FilePath installer_dir(
      installer_state.GetInstallerDirectory(new_version));
  install_list->AddCreateDirWorkItem(installer_dir);

  base::FilePath exe_dst(installer_dir.Append(setup_path.BaseName()));

  if (exe_dst != setup_path) {
    install_list->AddCopyTreeWorkItem(setup_path, exe_dst, temp_path,
                                      WorkItem::ALWAYS);
  }

  if (installer_state.RequiresActiveSetup()) {
    // Make a copy of setup.exe with a different name so that Active Setup
    // doesn't require an admin on XP thanks to Application Compatibility.
    base::FilePath active_setup_exe(installer_dir.Append(kActiveSetupExe));
    install_list->AddCopyTreeWorkItem(setup_path, active_setup_exe, temp_path,
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
      install_list->AddMoveTreeWorkItem(archive_path, archive_dst, temp_path,
                                        WorkItem::ALWAYS_MOVE);
    } else {
      // This may occur when setup is run out of an existing installation
      // directory. We cannot remove the system-level archive.
      install_list->AddCopyTreeWorkItem(archive_path, archive_dst, temp_path,
                                        WorkItem::ALWAYS);
    }
  }
}

// A callback invoked by |work_item| that adds firewall rules for Chrome. Rules
// are left in-place on rollback unless |remove_on_rollback| is true. This is
// the case for new installs only. Updates and overinstalls leave the rule
// in-place on rollback since a previous install of Chrome will be used in that
// case.
bool AddFirewallRulesCallback(const base::FilePath& chrome_path,
                              const CallbackWorkItem& work_item) {
  std::unique_ptr<FirewallManager> manager =
      FirewallManager::Create(chrome_path);
  if (!manager) {
    LOG(ERROR) << "Failed creating a FirewallManager. Continuing with install.";
    return true;
  }

  // Adding the firewall rule is expected to fail for user-level installs on
  // Vista+. Try anyway in case the installer is running elevated.
  if (!manager->AddFirewallRules())
    LOG(ERROR) << "Failed creating a firewall rules. Continuing with install.";

  // Don't abort installation if the firewall rule couldn't be added.
  return true;
}

// A callback invoked by |work_item| that removes firewall rules on rollback
// if this is a new install.
void RemoveFirewallRulesCallback(const base::FilePath& chrome_path,
                                 const CallbackWorkItem& work_item) {
  std::unique_ptr<FirewallManager> manager =
      FirewallManager::Create(chrome_path);
  if (!manager) {
    LOG(ERROR) << "Failed creating a FirewallManager. Continuing rollback.";
    return;
  }

  manager->RemoveFirewallRules();
}

// Adds work items to |list| to create firewall rules.
void AddFirewallRulesWorkItems(const InstallerState& installer_state,
                               bool is_new_install,
                               WorkItemList* list) {
  base::FilePath chrome_path = installer_state.target_path().Append(kChromeExe);
  WorkItem* item = list->AddCallbackWorkItem(
      base::BindOnce(&AddFirewallRulesCallback, chrome_path),
      base::BindOnce(&RemoveFirewallRulesCallback, chrome_path));
  item->set_rollback_enabled(is_new_install);
}

// Probes COM machinery to get an instance of notification_helper.exe's
// NotificationActivator class.
//
// This is required so that COM purges its cache of the path to the binary,
// which changes on updates.
bool ProbeNotificationActivatorCallback(const CLSID& toast_activator_clsid,
                                        const CallbackWorkItem& work_item) {
  DCHECK(toast_activator_clsid != CLSID_NULL);

  Microsoft::WRL::ComPtr<IUnknown> notification_activator;
  HRESULT hr =
      ::CoCreateInstance(toast_activator_clsid, nullptr, CLSCTX_LOCAL_SERVER,
                         IID_PPV_ARGS(&notification_activator));

  if (hr != REGDB_E_CLASSNOTREG) {
    LOG(ERROR) << "Unexpected result creating NotificationActivator; hr=0x"
               << std::hex << hr;
    return false;
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
  std::wstring uninstall_reg = install_static::GetUninstallRegistryPath();

  WorkItem* delete_reg_key = work_item_list->AddDeleteRegKeyWorkItem(
      reg_root, uninstall_reg, KEY_WOW64_32KEY);
  delete_reg_key->set_best_effort(true);
}

// Adds Chrome specific install work items to |install_list|.
void AddChromeWorkItems(const InstallParams& install_params,
                        WorkItemList* install_list) {
  const InstallerState& installer_state = *install_params.installer_state;
  const base::FilePath& archive_path = *install_params.archive_path;
  const base::FilePath& src_path = *install_params.src_path;
  const base::FilePath& temp_path = *install_params.temp_path;
  const base::Version& current_version = *install_params.current_version;
  const base::Version& new_version = *install_params.new_version;

  const base::FilePath& target_path = installer_state.target_path();

  if (current_version.IsValid()) {
    // Delete the archive from an existing install to save some disk space.
    base::FilePath old_installer_dir(
        installer_state.GetInstallerDirectory(current_version));
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

  install_list->AddCopyTreeWorkItem(src_path.Append(installer::kChromeExe),
                                    target_path.Append(installer::kChromeExe),
                                    temp_path, WorkItem::NEW_NAME_IF_IN_USE,
                                    new_chrome_exe);

  // Install kVisualElementsManifest if it is present in |src_path|. No need to
  // make this a conditional work item as if the file is not there now, it will
  // never be.
  // TODO(grt): Touch the Start Menu shortcut after putting the manifest in
  // place to force the Start Menu to refresh Chrome's tile.
  if (base::PathExists(src_path.Append(installer::kVisualElementsManifest))) {
    install_list->AddMoveTreeWorkItem(
        src_path.Append(installer::kVisualElementsManifest),
        target_path.Append(installer::kVisualElementsManifest), temp_path,
        WorkItem::ALWAYS_MOVE);
  } else {
    // We do not want to have an old VisualElementsManifest pointing to an old
    // version directory. Delete it as there wasn't a new one to replace it.
    install_list->AddDeleteTreeWorkItem(
        target_path.Append(installer::kVisualElementsManifest), temp_path);
  }

  // In the past, we copied rather than moved for system level installs so that
  // the permissions of %ProgramFiles% would be picked up.  Now that |temp_path|
  // is in %ProgramFiles% for system level installs (and in %LOCALAPPDATA%
  // otherwise), there is no need to do this.
  // Note that we pass true for check_duplicates to avoid failing on in-use
  // repair runs if the current_version is the same as the new_version.
  bool check_for_duplicates =
      (current_version.IsValid() && current_version == new_version);
  install_list->AddMoveTreeWorkItem(
      src_path.AppendASCII(new_version.GetString()),
      target_path.AppendASCII(new_version.GetString()), temp_path,
      check_for_duplicates ? WorkItem::CHECK_DUPLICATES
                           : WorkItem::ALWAYS_MOVE);

  // Delete any old_chrome.exe if present (ignore failure if it's in use).
  install_list
      ->AddDeleteTreeWorkItem(target_path.Append(installer::kChromeOldExe),
                              temp_path)
      ->set_best_effort(true);
}

// Adds work items to register the Elevation Service with Windows. Only for
// system level installs.
void AddElevationServiceWorkItems(const base::FilePath& elevation_service_path,
                                  WorkItemList* list) {
  DCHECK(::IsUserAnAdmin());

  if (elevation_service_path.empty()) {
    LOG(DFATAL) << "The path to elevation_service.exe is invalid.";
    return;
  }

  WorkItem* install_service_work_item = new InstallServiceWorkItem(
      install_static::GetElevationServiceName(),
      install_static::GetElevationServiceDisplayName(),
      GetLocalizedStringF(IDS_ELEVATION_SERVICE_DESCRIPTION_BASE,
                          {install_static::GetBaseAppName()}),
      SERVICE_DEMAND_START, base::CommandLine(elevation_service_path),
      base::CommandLine(base::CommandLine::NO_PROGRAM),
      install_static::GetClientStateKeyPath(),
      {install_static::GetElevatorClsid()}, {install_static::GetElevatorIid()});
  install_service_work_item->set_best_effort(true);
  list->AddWorkItem(install_service_work_item);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Adds work items to add the "store-dmtoken" command to Chrome's version key.
// This method is a no-op if this is anything other than system-level Chrome.
// The command is used when enrolling Chrome browser instances into enterprise
// management.
void AddEnterpriseEnrollmentWorkItems(const InstallerState& installer_state,
                                      const base::FilePath& setup_path,
                                      const base::Version& new_version,
                                      WorkItemList* install_list) {
  if (!installer_state.system_install())
    return;

  // Register a command to allow Chrome to request Google Update to run
  // setup.exe --store-dmtoken=<token>, which will store the specified token
  // in the registry.
  base::CommandLine cmd_line(installer_state.GetInstallerDirectory(new_version)
                                 .Append(setup_path.BaseName()));
  cmd_line.AppendSwitchASCII(switches::kStoreDMToken, "%1");
  cmd_line.AppendSwitch(switches::kSystemLevel);
  cmd_line.AppendSwitch(switches::kVerboseLogging);
  InstallUtil::AppendModeAndChannelSwitches(&cmd_line);

  // The substitution for the insert sequence "%1" here is performed safely by
  // Google Update rather than insecurely by the Windows shell. Disable the
  // safety check for unsafe insert sequences since the right thing is
  // happening. Do not blindly copy this pattern in new code. Check with a
  // member of base/win/OWNERS if in doubt.
  AppCommand cmd(kCmdStoreDMToken,
                 cmd_line.GetCommandLineStringWithUnsafeInsertSequences());

  // TODO(rogerta): For now setting this command as web accessible is required
  // by Google Update.  Could revisit this should Google Update change the
  // way permissions are handled for commands.
  cmd.set_is_web_accessible(true);
  cmd.AddCreateAppCommandWorkItems(installer_state.root_key(), install_list);
}

// Adds work items to add the "delete-dmtoken" command to Chrome's version key.
// This method is a no-op if this is anything other than system-level Chrome.
// The command is used when unenrolling Chrome browser instances from enterprise
// management.
void AddEnterpriseUnenrollmentWorkItems(const InstallerState& installer_state,
                                        const base::FilePath& setup_path,
                                        const base::Version& new_version,
                                        WorkItemList* install_list) {
  if (!installer_state.system_install())
    return;

  // Register a command to allow Chrome to request Google Update to run
  // setup.exe --delete-dmtoken, which will delete any existing DMToken from the
  // registry.
  base::CommandLine cmd_line(installer_state.GetInstallerDirectory(new_version)
                                 .Append(setup_path.BaseName()));
  cmd_line.AppendSwitch(switches::kDeleteDMToken);
  cmd_line.AppendSwitch(switches::kSystemLevel);
  cmd_line.AppendSwitch(switches::kVerboseLogging);
  InstallUtil::AppendModeAndChannelSwitches(&cmd_line);
  AppCommand cmd(kCmdDeleteDMToken, cmd_line.GetCommandLineString());

  // TODO(rogerta): For now setting this command as web accessible is required
  // by Google Update.  Could revisit this should Google Update change the
  // way permissions are handled for commands.
  cmd.set_is_web_accessible(true);
  cmd.AddCreateAppCommandWorkItems(installer_state.root_key(), install_list);
}

// Adds work items to add the "rotate-dtkey" command to Chrome's version key.
// This method is a no-op if this is anything other than system-level Chrome.
// The command is used to rotate the device signing key stored in HKLM.
void AddEnterpriseDeviceTrustWorkItems(const InstallerState& installer_state,
                                       const base::FilePath& setup_path,
                                       const base::Version& new_version,
                                       WorkItemList* install_list) {
  if (!installer_state.system_install())
    return;

  // Register a command to allow Chrome to request Google Update to run
  // setup.exe --rotate-dtkey=<dm-token>, which will rotate the key and store
  // it in the registry.
  base::CommandLine cmd_line(installer_state.GetInstallerDirectory(new_version)
                                 .Append(setup_path.BaseName()));
  cmd_line.AppendSwitchASCII(switches::kRotateDeviceTrustKey, "%1");
  cmd_line.AppendSwitchASCII(switches::kDmServerUrl, "%2");
  cmd_line.AppendSwitchASCII(switches::kNonce, "%3");
  cmd_line.AppendSwitch(switches::kSystemLevel);
  cmd_line.AppendSwitch(switches::kVerboseLogging);
  InstallUtil::AppendModeAndChannelSwitches(&cmd_line);

  // The substitution for the insert sequence "%1" here is performed safely by
  // Google Update rather than insecurely by the Windows shell. Disable the
  // safety check for unsafe insert sequences since the right thing is
  // happening. Do not blindly copy this pattern in new code. Check with a
  // member of base/win/OWNERS if in doubt.
  AppCommand cmd(kCmdRotateDeviceTrustKey,
                 cmd_line.GetCommandLineStringWithUnsafeInsertSequences());

  // TODO(rogerta): For now setting this command as web accessible is required
  // by Google Update.  Could revisit this should Google Update change the
  // way permissions are handled for commands.
  cmd.set_is_web_accessible(true);
  cmd.AddCreateAppCommandWorkItems(installer_state.root_key(), install_list);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

// This method adds work items to create (or update) Chrome uninstall entry in
// either the Control Panel->Add/Remove Programs list or in the Omaha client
// state key if running under an MSI installer.
void AddUninstallShortcutWorkItems(const InstallParams& install_params,
                                   WorkItemList* install_list) {
  const InstallerState& installer_state = *install_params.installer_state;
  const base::FilePath& setup_path = *install_params.setup_path;
  const base::Version& new_version = *install_params.new_version;

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

  std::wstring update_state_key(install_static::GetClientStateKeyPath());
  install_list->AddCreateRegKeyWorkItem(reg_root, update_state_key,
                                        KEY_WOW64_32KEY);
  install_list->AddSetRegValueWorkItem(
      reg_root, update_state_key, KEY_WOW64_32KEY,
      installer::kUninstallStringField, installer_path.value(), true);
  install_list->AddSetRegValueWorkItem(
      reg_root, update_state_key, KEY_WOW64_32KEY,
      installer::kUninstallArgumentsField,
      uninstall_arguments.GetCommandLineString(), true);

  // MSI installations will manage their own uninstall shortcuts.
  if (!installer_state.is_msi()) {
    // We need to quote the command line for the Add/Remove Programs dialog.
    base::CommandLine quoted_uninstall_cmd(installer_path);
    DCHECK_EQ(quoted_uninstall_cmd.GetCommandLineString()[0], '"');
    quoted_uninstall_cmd.AppendArguments(uninstall_arguments, false);

    std::wstring uninstall_reg = install_static::GetUninstallRegistryPath();
    install_list->AddCreateRegKeyWorkItem(reg_root, uninstall_reg,
                                          KEY_WOW64_32KEY);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY,
                                         installer::kUninstallDisplayNameField,
                                         InstallUtil::GetDisplayName(), true);
    install_list->AddSetRegValueWorkItem(
        reg_root, uninstall_reg, KEY_WOW64_32KEY,
        installer::kUninstallStringField,
        quoted_uninstall_cmd.GetCommandLineString(), true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY, L"InstallLocation",
                                         install_path.value(), true);

    std::wstring chrome_icon = ShellUtil::FormatIconLocation(
        install_path.Append(kChromeExe),
        install_static::GetAppIconResourceIndex());
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY, L"DisplayIcon",
                                         chrome_icon, true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY, L"NoModify",
                                         static_cast<DWORD>(1), true);
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY, L"NoRepair",
                                         static_cast<DWORD>(1), true);

    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY, L"Publisher",
                                         InstallUtil::GetPublisherName(), true);
    install_list->AddSetRegValueWorkItem(
        reg_root, uninstall_reg, KEY_WOW64_32KEY, L"Version",
        ASCIIToWide(new_version.GetString()), true);
    install_list->AddSetRegValueWorkItem(
        reg_root, uninstall_reg, KEY_WOW64_32KEY, L"DisplayVersion",
        ASCIIToWide(new_version.GetString()), true);
    // TODO(wfh): Ensure that this value is preserved in the 64-bit hive when
    // 64-bit installs place the uninstall information into the 64-bit registry.
    install_list->AddSetRegValueWorkItem(reg_root, uninstall_reg,
                                         KEY_WOW64_32KEY, L"InstallDate",
                                         InstallUtil::GetCurrentDate(), true);

    const std::vector<uint32_t>& version_components = new_version.components();
    if (version_components.size() == 4) {
      // Our version should be in major.minor.build.rev.
      install_list->AddSetRegValueWorkItem(
          reg_root, uninstall_reg, KEY_WOW64_32KEY, L"VersionMajor",
          static_cast<DWORD>(version_components[2]), true);
      install_list->AddSetRegValueWorkItem(
          reg_root, uninstall_reg, KEY_WOW64_32KEY, L"VersionMinor",
          static_cast<DWORD>(version_components[3]), true);
    }
  }
}

// Create Version key for a product (if not already present) and sets the new
// product version as the last step.
void AddVersionKeyWorkItems(const InstallParams& install_params,
                            WorkItemList* list) {
  const InstallerState& installer_state = *install_params.installer_state;
  const HKEY root = installer_state.root_key();

  // Only set "lang" for user-level installs since for system-level, the install
  // language may not be related to a given user's runtime language.
  const bool add_language_identifier = !installer_state.system_install();

  const std::wstring clients_key = install_static::GetClientsKeyPath();
  list->AddCreateRegKeyWorkItem(root, clients_key, KEY_WOW64_32KEY);

  list->AddSetRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                               google_update::kRegNameField,
                               InstallUtil::GetDisplayName(),
                               true);  // overwrite name also

  // Clean up when updating from M85 and older installs.
  // Can be removed after newer stable builds have been in the wild
  // enough to have done a reasonable degree of clean up.
  list->AddDeleteRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                                  L"oopcrashes");

  if (add_language_identifier) {
    // Write the language identifier of the current translation.  Omaha's set of
    // languages is a superset of Chrome's set of translations with this one
    // exception: what Chrome calls "en-us", Omaha calls "en".  sigh.
    std::wstring language(GetCurrentTranslation());
    if (base::EqualsCaseInsensitiveASCII(language, "en-us"))
      language.resize(2);
    list->AddSetRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                                 google_update::kRegLangField, language,
                                 false);  // do not overwrite language
  }
  list->AddSetRegValueWorkItem(
      root, clients_key, KEY_WOW64_32KEY, google_update::kRegVersionField,
      ASCIIToWide(install_params.new_version->GetString()),
      true);  // overwrite version
}

void AddUpdateBrandCodeWorkItem(const InstallerState& installer_state,
                                WorkItemList* install_list) {
  // Only update specific brand codes needed for enterprise.
  std::wstring brand;
  if (!GoogleUpdateSettings::GetBrand(&brand))
    return;

  // Only update if this machine is a managed device, including domain join.
  if (!base::IsManagedDevice()) {
    return;
  }

  std::wstring new_brand = GetUpdatedBrandCode(brand);
  // Rewrite the old brand so that the next step can potentially apply both
  // changes at once.
  if (!new_brand.empty()) {
    brand = new_brand;
  }

  // Furthermore do the CBCM brand code conversion both ways.
  base::win::RegKey key;
  std::wstring value_name;
  bool has_valid_dm_token = false;
  std::tie(key, value_name) = InstallUtil::GetCloudManagementDmTokenLocation(
      InstallUtil::ReadOnly(true), InstallUtil::BrowserLocation(false));
  if (key.Valid()) {
    DWORD dtype = REG_NONE;
    std::vector<char> raw_value(512);
    DWORD size = static_cast<DWORD>(raw_value.size());
    auto result =
        key.ReadValue(value_name.c_str(), raw_value.data(), &size, &dtype);
    if (result == ERROR_MORE_DATA && size > raw_value.size()) {
      raw_value.resize(size);
      result =
          key.ReadValue(value_name.c_str(), raw_value.data(), &size, &dtype);
    }
    if (result == ERROR_SUCCESS && dtype == REG_BINARY && size != 0) {
      std::string dmtoken_value(base::TrimWhitespaceASCII(
          std::string_view(raw_value.data(), size), base::TRIM_ALL));
      if (dmtoken_value.compare("INVALID_DM_TOKEN")) {
        has_valid_dm_token = true;
      }
    }
  }

  bool is_cbcm_enrolled =
      !InstallUtil::GetCloudManagementEnrollmentToken().empty() ||
      has_valid_dm_token;
  std::wstring cbcm_brand =
      TransformCloudManagementBrandCode(brand, /*to_cbcm=*/is_cbcm_enrolled);
  if (!cbcm_brand.empty()) {
    new_brand = cbcm_brand;
  }

  if (new_brand.empty()) {
    return;
  }

  install_list->AddSetRegValueWorkItem(
      installer_state.root_key(), install_static::GetClientStateKeyPath(),
      KEY_WOW64_32KEY, google_update::kRegRLZBrandField, new_brand, true);
}

std::wstring GetUpdatedBrandCode(const std::wstring& brand_code) {
  // Brand codes to be remapped on enterprise installs.
  static constexpr struct EnterpriseBrandRemapping {
    const wchar_t* old_brand;
    const wchar_t* new_brand;
  } kEnterpriseBrandRemapping[] = {
      {L"GGLS", L"GCEU"},
      {L"GGRV", L"GCEV"},
      {L"GTPM", L"GCER"},
  };

  for (auto mapping : kEnterpriseBrandRemapping) {
    if (brand_code == mapping.old_brand)
      return mapping.new_brand;
  }
  return std::wstring();
}

std::wstring TransformCloudManagementBrandCode(const std::wstring& brand_code,
                                               bool to_cbcm) {
  // Brand codes to be remapped on enterprise installs.
  // We are extracting the 4th letter below so we should better have one.
  if (brand_code.length() != 4 || brand_code == L"GCEL") {
    return std::wstring();
  }
  static constexpr struct CbcmBrandRemapping {
    const wchar_t* cbe_brand;
    const wchar_t* cbcm_brand;
  } kCbcmBrandRemapping[] = {
      {L"GCE", L"GCC"}, {L"GCF", L"GCK"}, {L"GCG", L"GCL"}, {L"GCH", L"GCM"},
      {L"GCO", L"GCT"}, {L"GCP", L"GCU"}, {L"GCQ", L"GCV"}, {L"GCS", L"GCW"},
  };
  if (to_cbcm) {
    for (auto mapping : kCbcmBrandRemapping) {
      if (base::StartsWith(brand_code, mapping.cbe_brand,
                           base::CompareCase::SENSITIVE)) {
        return std::wstring(mapping.cbcm_brand) + brand_code[3];
      }
    }
  } else {
    for (auto mapping : kCbcmBrandRemapping) {
      if (base::StartsWith(brand_code, mapping.cbcm_brand,
                           base::CompareCase::SENSITIVE)) {
        return std::wstring(mapping.cbe_brand) + brand_code[3];
      }
    }
  }
  return std::wstring();
}

bool AppendPostInstallTasks(const InstallParams& install_params,
                            WorkItemList* post_install_task_list) {
  DCHECK(post_install_task_list);

  const InstallerState& installer_state = *install_params.installer_state;
  const base::FilePath& setup_path = *install_params.setup_path;
  const base::FilePath& src_path = *install_params.src_path;
  const base::FilePath& temp_path = *install_params.temp_path;
  const base::Version& current_version = *install_params.current_version;
  const base::Version& new_version = *install_params.new_version;

  HKEY root = installer_state.root_key();
  const base::FilePath& target_path = installer_state.target_path();
  base::FilePath new_chrome_exe(target_path.Append(kChromeNewExe));
  const std::wstring clients_key(install_static::GetClientsKeyPath());

  base::FilePath installer_path(
      installer_state.GetInstallerDirectory(new_version)
          .Append(setup_path.BaseName()));

  // Append work items that will only be executed if this was an in-use update.
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
    base::Version critical_version(
        installer_state.DetermineCriticalVersion(current_version, new_version));

    if (current_version.IsValid()) {
      in_use_update_work_items->AddSetRegValueWorkItem(
          root, clients_key, KEY_WOW64_32KEY,
          google_update::kRegOldVersionField,
          ASCIIToWide(current_version.GetString()), true);
    }
    if (critical_version.IsValid()) {
      in_use_update_work_items->AddSetRegValueWorkItem(
          root, clients_key, KEY_WOW64_32KEY,
          google_update::kRegCriticalVersionField,
          ASCIIToWide(critical_version.GetString()), true);
    } else {
      in_use_update_work_items->AddDeleteRegValueWorkItem(
          root, clients_key, KEY_WOW64_32KEY,
          google_update::kRegCriticalVersionField);
    }

    // Form the mode-specific rename command and register it.
    base::CommandLine product_rename_cmd(installer_path);
    product_rename_cmd.AppendSwitch(switches::kRenameChromeExe);
    if (installer_state.system_install())
      product_rename_cmd.AppendSwitch(switches::kSystemLevel);
    if (installer_state.verbose_logging())
      product_rename_cmd.AppendSwitch(switches::kVerboseLogging);
    InstallUtil::AppendModeAndChannelSwitches(&product_rename_cmd);
    AppCommand(installer::kCmdRenameChromeExe,
               product_rename_cmd.GetCommandLineString())
        .AddCreateAppCommandWorkItems(root, in_use_update_work_items.get());
    // Some clients in Chrome 110 look for an alternate rename command id. Write
    // that one as well so those can find it and be able to finish updating.
    // TODO(floresa): Remove all uses of the alternate id in Chrome 111.
    AppCommand(installer::kCmdAlternateRenameChromeExe,
               product_rename_cmd.GetCommandLineString())
        .AddCreateAppCommandWorkItems(root, in_use_update_work_items.get());

    if (!installer_state.system_install()) {
      // Chrome versions prior to 110.0.5435.0 still look for the User rename
      // command line REG_SZ "cmd" under the path
      // "Software\Google\Update\Clients\<guid>" where "<guid>" is the current
      // install mode's appguid.
      in_use_update_work_items->AddSetRegValueWorkItem(
          root, clients_key, KEY_WOW64_32KEY, installer::kCmdRenameChromeExe,
          product_rename_cmd.GetCommandLineString(), true);
    }

    // Delay deploying the new chrome_proxy while chrome is running.
    in_use_update_work_items->AddCopyTreeWorkItem(
        src_path.Append(kChromeProxyExe),
        target_path.Append(kChromeProxyNewExe), temp_path, WorkItem::ALWAYS);

    post_install_task_list->AddWorkItem(in_use_update_work_items.release());
  }

  // Append work items that will be executed if this was NOT an in-use update.
  {
    std::unique_ptr<WorkItemList> regular_update_work_items(
        WorkItem::CreateConditionalWorkItemList(
            new Not(new ConditionRunIfFileExists(new_chrome_exe))));
    regular_update_work_items->set_log_message("RegularUpdateWorkItemList");

    // If a channel was specified by policy, update the "channel" registry value
    // with it so that the browser knows which channel to use, otherwise delete
    // whatever value that key holds.
    AddChannelWorkItems(root, clients_key, regular_update_work_items.get());
    AddFinalizeUpdateWorkItems(new_version, installer_state, installer_path,
                               regular_update_work_items.get());

    // Since this was not an in-use-update, delete 'opv', 'cpv',
    // and 'cmd' keys.
    regular_update_work_items->AddDeleteRegValueWorkItem(
        root, clients_key, KEY_WOW64_32KEY, google_update::kRegOldVersionField);
    regular_update_work_items->AddDeleteRegValueWorkItem(
        root, clients_key, KEY_WOW64_32KEY,
        google_update::kRegCriticalVersionField);
    AppCommand(installer::kCmdRenameChromeExe, {})
        .AddDeleteAppCommandWorkItems(root, regular_update_work_items.get());
    AppCommand(installer::kCmdAlternateRenameChromeExe, {})
        .AddDeleteAppCommandWorkItems(root, regular_update_work_items.get());

    if (!installer_state.system_install()) {
      regular_update_work_items->AddDeleteRegValueWorkItem(
          root, clients_key, KEY_WOW64_32KEY, installer::kCmdRenameChromeExe);
    }

    // Only copy chrome_proxy.exe directly when chrome.exe isn't in use to avoid
    // different versions getting mixed up between the two binaries.
    regular_update_work_items->AddCopyTreeWorkItem(
        src_path.Append(kChromeProxyExe), target_path.Append(kChromeProxyExe),
        temp_path, WorkItem::ALWAYS);

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

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  // Add a best-effort item to create the ClientStateMedium key for system-level
  // installs. This is ordinarily done by Google Update prior to running
  // Chrome's installer. Do it here as well so that the key exists for manual
  // installs.
  if (installer_state.system_install()) {
    const std::wstring path = install_static::GetClientStateMediumKeyPath();
    post_install_task_list
        ->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, path, KEY_WOW64_32KEY)
        ->set_best_effort(true);
  }

  // Apply policy-driven channel selection to the "ap" value for subsequent
  // update checks even if the policy is cleared.
  AddChannelSelectionWorkItems(installer_state, post_install_task_list);
#endif  // BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)

  return true;
}

void AddInstallWorkItems(const InstallParams& install_params,
                         WorkItemList* install_list) {
  DCHECK(install_list);

  const InstallerState& installer_state = *install_params.installer_state;
  const base::FilePath& setup_path = *install_params.setup_path;
  const base::FilePath& temp_path = *install_params.temp_path;
  const base::Version& current_version = *install_params.current_version;
  const base::Version& new_version = *install_params.new_version;

  const base::FilePath& target_path = installer_state.target_path();

  // A temp directory that work items need and the actual install directory.
  install_list->AddCreateDirWorkItem(temp_path);
  install_list->AddCreateDirWorkItem(target_path);

  // Set permissions early on both temp and target, since moved files may not
  // inherit permissions.
  WorkItem* add_ac_acl_to_install = install_list->AddCallbackWorkItem(
      base::BindOnce(
          [](const base::FilePath& target_path, const base::FilePath& temp_path,
             const CallbackWorkItem& work_item) {
            return ConfigureAppContainerSandbox(
                std::array<const base::FilePath*, 2>{&target_path, &temp_path});
          },
          target_path, temp_path),
      base::DoNothing());
  add_ac_acl_to_install->set_best_effort(true);
  add_ac_acl_to_install->set_rollback_enabled(false);

  // Create the directory in which persistent metrics will be stored.
  const base::FilePath histogram_storage_dir(
      target_path.AppendASCII(kSetupHistogramAllocatorName));
  install_list->AddCreateDirWorkItem(histogram_storage_dir);

  if (installer_state.system_install()) {
    WorkItem* add_acl_to_histogram_storage_dir_work_item =
        install_list->AddCallbackWorkItem(
            base::BindOnce(
                [](const base::FilePath& histogram_storage_dir,
                   const CallbackWorkItem& work_item) {
                  return base::win::GrantAccessToPath(
                      histogram_storage_dir,
                      base::win::Sid::FromKnownSidVector(
                          {base::win::WellKnownSid::kAuthenticatedUser}),
                      FILE_GENERIC_READ | FILE_DELETE_CHILD,
                      CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE);
                },
                histogram_storage_dir),
            base::DoNothing());
    add_acl_to_histogram_storage_dir_work_item->set_best_effort(true);
    add_acl_to_histogram_storage_dir_work_item->set_rollback_enabled(false);
  }

  AddChromeWorkItems(install_params, install_list);

  // Copy installer in install directory
  AddInstallerCopyTasks(install_params, install_list);

  AddUninstallShortcutWorkItems(install_params, install_list);

  AddVersionKeyWorkItems(install_params, install_list);

  AddCleanupDeprecatedPerUserRegistrationsWorkItems(install_list);

  AddActiveSetupWorkItems(installer_state, new_version, install_list);

  AddOsUpgradeWorkItems(installer_state, setup_path, new_version, install_list);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddEnterpriseEnrollmentWorkItems(installer_state, setup_path, new_version,
                                   install_list);
  AddEnterpriseUnenrollmentWorkItems(installer_state, setup_path, new_version,
                                     install_list);
  AddEnterpriseDeviceTrustWorkItems(installer_state, setup_path, new_version,
                                    install_list);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING
  AddFirewallRulesWorkItems(installer_state, !current_version.IsValid(),
                            install_list);

  // We don't have a version check for Win10+ here so that Windows upgrades
  // work.
  AddNativeNotificationWorkItems(
      installer_state.root_key(),
      GetNotificationHelperPath(target_path, new_version), install_list);

  AddUpdateDowngradeVersionItem(installer_state.root_key(), current_version,
                                new_version, install_list);

  AddUpdateBrandCodeWorkItem(installer_state, install_list);

  // Append the tasks that run after the installation.
  AppendPostInstallTasks(install_params, install_list);
}

void AddNativeNotificationWorkItems(
    HKEY root,
    const base::FilePath& notification_helper_path,
    WorkItemList* list) {
  if (notification_helper_path.empty()) {
    LOG(DFATAL) << "The path to notification_helper.exe is invalid.";
    return;
  }

  std::wstring toast_activator_reg_path =
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
  WorkItem* item = list->AddCallbackWorkItem(
      base::BindOnce(&ProbeNotificationActivatorCallback,
                     install_static::GetToastActivatorClsid()),
      base::BindOnce(base::IgnoreResult(&ProbeNotificationActivatorCallback),
                     install_static::GetToastActivatorClsid()));
  item->set_best_effort(true);

  std::wstring toast_activator_server_path =
      toast_activator_reg_path + L"\\LocalServer32";

  // Command-line featuring the quoted path to the exe.
  std::wstring command(1, L'"');
  command.append(notification_helper_path.value()).append(1, L'"');

  list->AddCreateRegKeyWorkItem(root, toast_activator_server_path,
                                WorkItem::kWow64Default);

  list->AddSetRegValueWorkItem(root, toast_activator_server_path,
                               WorkItem::kWow64Default, L"", command, true);

  list->AddSetRegValueWorkItem(root, toast_activator_server_path,
                               WorkItem::kWow64Default, L"ServerExecutable",
                               notification_helper_path.value(), true);
}

void AddOldWerHelperRegistrationCleanupItems(HKEY root,
                                             const base::FilePath& target_path,
                                             WorkItemList* list) {
  std::wstring value_prefix(target_path.value());
  DCHECK(!value_prefix.empty());
  if (value_prefix.back() != L'\\')
    value_prefix.push_back(L'\\');
  const std::wstring value_postfix(std::wstring(L"\\") + kWerDll);
  const std::wstring wer_registry_path = GetWerHelperRegistryPath();
  for (base::win::RegistryValueIterator value_iter(
           root, wer_registry_path.c_str(), WorkItem::kWow64Default);
       value_iter.Valid(); ++value_iter) {
    const std::wstring value_name(value_iter.Name());
    if (value_name.size() <= value_prefix.size() + value_postfix.size())
      continue;

    if (base::StartsWith(value_name, value_prefix,
                         base::CompareCase::INSENSITIVE_ASCII) &&
        base::EndsWith(value_name, value_postfix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      std::wstring value_version = value_name.substr(
          value_prefix.size(),
          value_name.size() - value_prefix.size() - value_postfix.size());
      if (base::Version(base::WideToASCII(value_version)).IsValid()) {
        list->AddDeleteRegValueWorkItem(root, wer_registry_path,
                                        WorkItem::kWow64Default, value_name);
      }
    }
  }
}

void AddWerHelperRegistration(HKEY root,
                              const base::FilePath& wer_helper_path,
                              WorkItemList* list) {
  DCHECK(!wer_helper_path.empty());

  std::wstring wer_registry_path = GetWerHelperRegistryPath();

  list->AddCreateRegKeyWorkItem(root, wer_registry_path,
                                WorkItem::kWow64Default);

  // The DWORD value is not important.
  list->AddSetRegValueWorkItem(root, wer_registry_path, WorkItem::kWow64Default,
                               wer_helper_path.value().c_str(), DWORD{0},
                               /*overwrite=*/true);
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
  const std::wstring active_setup_path(install_static::GetActiveSetupPath());

  VLOG(1) << "Adding registration items for Active Setup.";
  list->AddCreateRegKeyWorkItem(root, active_setup_path,
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, active_setup_path, WorkItem::kWow64Default,
                               L"", InstallUtil::GetDisplayName(), true);

  base::FilePath active_setup_exe(
      installer_state.GetInstallerDirectory(new_version)
          .Append(kActiveSetupExe));
  base::CommandLine cmd(active_setup_exe);
  cmd.AppendSwitch(installer::switches::kConfigureUserSettings);
  cmd.AppendSwitch(installer::switches::kVerboseLogging);
  cmd.AppendSwitch(installer::switches::kSystemLevel);
  InstallUtil::AppendModeAndChannelSwitches(&cmd);
  list->AddSetRegValueWorkItem(root, active_setup_path, WorkItem::kWow64Default,
                               L"StubPath", cmd.GetCommandLineString(), true);

  // TODO(grt): http://crbug.com/75152 Write a reference to a localized
  // resource.
  list->AddSetRegValueWorkItem(root, active_setup_path, WorkItem::kWow64Default,
                               L"Localized Name", InstallUtil::GetDisplayName(),
                               true);

  list->AddSetRegValueWorkItem(root, active_setup_path, WorkItem::kWow64Default,
                               L"IsInstalled", static_cast<DWORD>(1U), true);

  list->AddWorkItem(new UpdateActiveSetupVersionWorkItem(
      active_setup_path, UpdateActiveSetupVersionWorkItem::UPDATE));
}

void AppendUninstallCommandLineFlags(const InstallerState& installer_state,
                                     base::CommandLine* uninstall_cmd) {
  DCHECK(uninstall_cmd);

  uninstall_cmd->AppendSwitch(installer::switches::kUninstall);

  InstallUtil::AppendModeAndChannelSwitches(uninstall_cmd);
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

  if (installer_state.operation() == InstallerState::UNINSTALL) {
    AppCommand(kCmdOnOsUpgrade, {})
        .AddDeleteAppCommandWorkItems(root_key, install_list);
  } else {
    // Register with Google Update to have setup.exe --on-os-upgrade called on
    // OS upgrade.
    base::CommandLine cmd_line(
        installer_state.GetInstallerDirectory(new_version)
            .Append(setup_path.BaseName()));
    // Add the main option to indicate OS upgrade flow.
    cmd_line.AppendSwitch(installer::switches::kOnOsUpgrade);
    InstallUtil::AppendModeAndChannelSwitches(&cmd_line);
    if (installer_state.system_install())
      cmd_line.AppendSwitch(installer::switches::kSystemLevel);
    // Log everything for now.
    cmd_line.AppendSwitch(installer::switches::kVerboseLogging);

    AppCommand cmd(kCmdOnOsUpgrade, cmd_line.GetCommandLineString());
    cmd.set_is_auto_run_on_os_upgrade(true);
    cmd.AddCreateAppCommandWorkItems(root_key, install_list);
  }
}

void AddChannelWorkItems(HKEY root,
                         const std::wstring& clients_key,
                         WorkItemList* list) {
  const auto& install_details = install_static::InstallDetails::Get();
  if (install_details.channel_origin() ==
      install_static::ChannelOrigin::kPolicy) {
    // Use channel_override rather than simply channel so that extended stable
    // is differentiated from regular.
    list->AddSetRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                                 google_update::kRegChannelField,
                                 install_details.channel_override(),
                                 /*overwrite=*/true);
  } else {
    list->AddDeleteRegValueWorkItem(root, clients_key, KEY_WOW64_32KEY,
                                    google_update::kRegChannelField);
  }
}

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
void AddChannelSelectionWorkItems(const InstallerState& installer_state,
                                  WorkItemList* list) {
  const auto& install_details = install_static::InstallDetails::Get();

  // Nothing to do if the channel wasn't selected via the command line switch.
  if (install_details.channel_origin() !=
      install_static::ChannelOrigin::kPolicy) {
    return;
  }

  auto item = std::make_unique<ChannelOverrideWorkItem>();
  item->set_best_effort(true);
  list->AddWorkItem(item.release());
}
#endif  // BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)

void AddFinalizeUpdateWorkItems(const base::Version& new_version,
                                const InstallerState& installer_state,
                                const base::FilePath& setup_path,
                                WorkItemList* list) {
  // Cleanup for breaking downgrade first in the post install to avoid
  // overwriting any of the following post-install tasks.
  AddDowngradeCleanupItems(new_version, list);

  const base::FilePath target_path = installer_state.target_path();
  AddOldWerHelperRegistrationCleanupItems(installer_state.root_key(),
                                          target_path, list);
  AddWerHelperRegistration(installer_state.root_key(),
                           GetWerHelperPath(target_path, new_version), list);

  if (installer_state.system_install()) {
    AddElevationServiceWorkItems(
        GetElevationServicePath(target_path, new_version), list);
  }

  const std::wstring client_state_key = install_static::GetClientStateKeyPath();

  // Adds the command that needs to be used in order to cleanup any breaking
  // changes the installer of this version may have added.
  list->AddSetRegValueWorkItem(
      installer_state.root_key(), client_state_key, KEY_WOW64_32KEY,
      google_update::kRegDowngradeCleanupCommandField,
      GetDowngradeCleanupCommandWithPlaceholders(setup_path, installer_state),
      true);

  // Write the latest installer's breaking version so that future downgrades
  // know if they need to do a clean install. This isn't done for in-use since
  // it is done at the the executable's rename.
  list->AddSetRegValueWorkItem(
      installer_state.root_key(), client_state_key, KEY_WOW64_32KEY,
      google_update::kRegCleanInstallRequiredForVersionBelowField,
      kLastBreakingInstallerVersion, true);

  // Remove any "experiment_labels" value that may have been set. Support for
  // this was removed in Q4 2023.
  list->AddDeleteRegValueWorkItem(
          installer_state.root_key(),
          installer_state.system_install()
              ? install_static::GetClientStateMediumKeyPath()
              : client_state_key,
          KEY_WOW64_32KEY, L"experiment_labels")
      ->set_best_effort(true);
}

}  // namespace installer
