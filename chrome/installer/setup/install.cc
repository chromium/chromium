// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install.h"

#include <windows.h>

#include <shlobj.h>
#include <time.h>

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/shortcut.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/install_params.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/installer_crash_reporting.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/update_active_setup_version_work_item.h"
#include "chrome/installer/util/beacons.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/delete_old_versions.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/taskbar_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"

namespace installer {

namespace {

void LogShortcutOperation(ShellUtil::ShortcutLocation location,
                          const ShellUtil::ShortcutProperties& properties,
                          ShellUtil::ShortcutOperation operation,
                          bool failed) {
  // ShellUtil::SHELL_SHORTCUT_UPDATE_EXISTING should not be used at install and
  // thus this method does not handle logging a message for it.
  DCHECK(operation != ShellUtil::SHELL_SHORTCUT_UPDATE_EXISTING);
  std::string message;
  if (failed)
    message.append("Failed: ");
  message.append(
      (operation == ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS ||
       operation == ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL)
          ? "Creating "
          : "Overwriting ");
  if (failed && operation == ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING)
    message.append("(maybe the shortcut doesn't exist?) ");
  message.append((properties.level == ShellUtil::CURRENT_USER) ? "per-user "
                                                               : "all-users ");
  switch (location) {
    case ShellUtil::SHORTCUT_LOCATION_DESKTOP:
      message.append("Desktop ");
      break;
    case ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH:
      message.append("Quick Launch ");
      break;
    case ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT:
      message.append("Start menu ");
      break;
    case ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED:
      NOTREACHED_IN_MIGRATION();
      break;
    case ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR:
      message.append(
          "Start menu/" +
          base::WideToUTF8(InstallUtil::GetChromeAppsShortcutDirName()) + " ");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  message.push_back('"');
  if (properties.has_shortcut_name())
    message.append(base::WideToUTF8(properties.shortcut_name));
  else
    message.append(base::WideToUTF8(InstallUtil::GetDisplayName()));
  message.push_back('"');

  message.append(" shortcut to ");
  message.append(base::WideToUTF8(properties.target.value()));
  if (properties.has_arguments())
    message.append(base::WideToUTF8(properties.arguments));

  if (properties.pin_to_taskbar && CanPinShortcutToTaskbar())
    message.append(" and pinning to the taskbar");

  message.push_back('.');

  if (failed)
    LOG(WARNING) << message;
  else
    VLOG(1) << message;
}

void ExecuteAndLogShortcutOperation(
    ShellUtil::ShortcutLocation location,
    const ShellUtil::ShortcutProperties& properties,
    ShellUtil::ShortcutOperation operation) {
  LogShortcutOperation(location, properties, operation, /*failed=*/false);
  bool pinned = false;
  bool success = ShellUtil::CreateOrUpdateShortcut(location, properties,
                                                   operation, &pinned);
  if (!success)
    LogShortcutOperation(location, properties, operation, /*failed=*/true);

  // For Start Menu shortcut creation on versions of Win10 that support
  // pinning, record whether or not the installer pinned Chrome.
  if (location == ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT &&
      CanPinShortcutToTaskbar()) {
    SetInstallerPinnedChromeToTaskbar(properties.pin_to_taskbar && pinned);
  }
}

void AddChromeToMediaPlayerList() {
  std::wstring reg_path(kMediaPlayerRegPath);
  // registry paths can also be appended like file system path
  reg_path.push_back(base::FilePath::kSeparators[0]);
  reg_path.append(kChromeExe);
  VLOG(1) << "Adding Chrome to Media player list at " << reg_path;
  std::unique_ptr<WorkItem> work_item(WorkItem::CreateCreateRegKeyWorkItem(
      HKEY_LOCAL_MACHINE, reg_path, WorkItem::kWow64Default));

  // if the operation fails we log the error but still continue
  if (!work_item.get()->Do())
    LOG(ERROR) << "Could not add Chrome to media player inclusion list.";
}

// Copy the initial preferences file provided to installer, in the same folder
// as chrome.exe so Chrome first run can find it. This function will be called
// only on the first install of Chrome.
void CopyPreferenceFileForFirstRun(const InstallerState& installer_state,
                                   const base::FilePath& prefs_source_path) {
  if (!base::CopyFile(prefs_source_path,
                      InitialPreferences::Path(installer_state.target_path(),
                                               /*for_read=*/false))) {
    VPLOG(1) << "Failed to copy initial preferences from \""
             << prefs_source_path << "\"";
  }
}

// This function installs a new version of Chrome to the specified location.
//
// install_params: See install_params.h
//
// This function makes best effort to do installation in a transactional
// manner. If failed it tries to rollback all changes on the file system
// and registry. For example, if package exists before calling the
// function, it rolls back all new file and directory changes under
// package. If package does not exist before calling the function
// (typical new install), the function creates package during install
// and removes the whole directory during rollback.
InstallStatus InstallNewVersion(const InstallParams& install_params,
                                bool is_downgrade_allowed) {
  const InstallerState& installer_state = *install_params.installer_state;
  const base::Version& current_version = *install_params.current_version;
  const base::Version& new_version = *install_params.new_version;

  installer_state.SetStage(BUILDING);

  SetCurrentVersionCrashKey(current_version);

  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());

  AddInstallWorkItems(install_params, install_list.get());

  base::FilePath new_chrome_exe(
      installer_state.target_path().Append(kChromeNewExe));

  installer_state.SetStage(EXECUTING);

  if (!install_list->Do()) {
    installer_state.SetStage(ROLLINGBACK);
    InstallStatus result = base::PathExists(new_chrome_exe) &&
                                   current_version.IsValid() &&
                                   new_version == current_version
                               ? SAME_VERSION_REPAIR_FAILED
                               : INSTALL_FAILED;
    LOG(ERROR) << "Install failed, rolling back... result: " << result;
    install_list->Rollback();
    LOG(ERROR) << "Rollback complete. ";
    return result;
  }

  if (!current_version.IsValid()) {
    VLOG(1) << "First install of version " << new_version;
    return FIRST_INSTALL_SUCCESS;
  }

  if (new_version == current_version) {
    VLOG(1) << "Install repaired of version " << new_version;
    return INSTALL_REPAIRED;
  }

  bool new_chrome_exe_exists = base::PathExists(new_chrome_exe);
  if (new_version > current_version) {
    if (new_chrome_exe_exists) {
      VLOG(1) << "Version updated to " << new_version << " while running "
              << current_version;
      return IN_USE_UPDATED;
    }
    VLOG(1) << "Version updated to " << new_version;
    return NEW_VERSION_UPDATED;
  }

  if (is_downgrade_allowed) {
    if (new_chrome_exe_exists) {
      VLOG(1) << "Version downgraded to " << new_version << " while running "
              << current_version;
      return IN_USE_DOWNGRADE;
    }
    VLOG(1) << "Version downgraded to " << new_version;
    return OLD_VERSION_DOWNGRADE;
  }

  LOG(ERROR) << "Not sure how we got here while updating"
             << ", new version: " << new_version
             << ", old version: " << current_version;

  return INSTALL_FAILED;
}

std::string GenerateVisualElementsManifest(const base::Version& version) {
  // A printf-style format string for generating the visual elements manifest.
  // Required arguments, in order, are thrice:
  //   - Relative path to the VisualElements directory.
  //   - Logo suffix for the channel.
  static constexpr char kManifestTemplate[] =
      "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
      "  <VisualElements\r\n"
      "      ShowNameOnSquare150x150Logo='on'\r\n"
      "      Square150x150Logo='%s\\Logo%s.png'\r\n"
      "      Square70x70Logo='%s\\SmallLogo%s.png'\r\n"
      "      Square44x44Logo='%s\\SmallLogo%s.png'\r\n"
      "      ForegroundText='light'\r\n"
      "      BackgroundColor='#5F6368'/>\r\n"
      "</Application>\r\n";

  // Construct the relative path to the versioned VisualElements directory.
  std::string elements_dir = version.GetString();
  elements_dir.push_back(
      base::checked_cast<char>(base::FilePath::kSeparators[0]));
  elements_dir.append(kVisualElements);

  // Fill the manifest with the desired values.
  const std::string logo_suffix =
      base::WideToUTF8(install_static::InstallDetails::Get().logo_suffix());
  return base::StringPrintf(kManifestTemplate, elements_dir.c_str(),
                            logo_suffix.c_str(), elements_dir.c_str(),
                            logo_suffix.c_str(), elements_dir.c_str(),
                            logo_suffix.c_str());
}

// Whether VisualElements assets exist for this brand and mode.
bool HasVisualElementAssets(const base::FilePath& base_path,
                            const base::Version& version) {
  // There are no assets at all if there's no VisualElements directory.
  base::FilePath visual_elements_dir =
      base_path.AppendASCII(version.GetString()).AppendASCII(kVisualElements);
  if (!base::DirectoryExists(visual_elements_dir)) {
    return false;
  }

// Assets are unconditionally required if there is a VisualElements directory.
#if DCHECK_IS_ON()
  DCHECK(base::PathExists(visual_elements_dir.Append(base::StrCat(
      {L"Logo", install_static::InstallDetails::Get().logo_suffix(),
       L".png"}))));
#endif

  return true;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void LaunchOSUpdateHandlerIfNeeded(const InstallerState& installer_state,
                                   const std::wstring& installed_version) {
  auto os_update_handler_cmd =
      GetOsUpdateHandlerCommand(installer_state, installed_version,
                                *base::CommandLine::ForCurrentProcess());
  if (!os_update_handler_cmd.has_value()) {
    return;
  }
  base::LaunchOptions launch_options;
  launch_options.feedback_cursor_off = true;
  launch_options.force_breakaway_from_job_ = true;

  ::SetLastError(ERROR_SUCCESS);
  base::Process process =
      base::LaunchProcess(os_update_handler_cmd.value(), launch_options);
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch \""
                << os_update_handler_cmd->GetCommandLineString() << "\"";
  }
  // There's no need to wait for this to finish.
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

bool CreateVisualElementsManifest(const base::FilePath& src_path,
                                  const base::Version& version) {
  if (!HasVisualElementAssets(src_path, version)) {
    VLOG(1) << "No visual elements found, not writing "
            << kVisualElementsManifest << " to " << src_path.value();
    return true;
  }

  // Generate the manifest.
  const std::string manifest(GenerateVisualElementsManifest(version));

  // Write the manifest to |src_path|.
  if (base::WriteFile(src_path.Append(kVisualElementsManifest), manifest)) {
    VLOG(1) << "Successfully wrote " << kVisualElementsManifest << " to "
            << src_path.value();
    return true;
  }
  PLOG(ERROR) << "Error writing " << kVisualElementsManifest << " to "
              << src_path.value();
  return false;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Returns a CommandLine to run if os_update_handler.exe should be run,
// i.e. a Windows update has been detected; null otherwise.
std::optional<base::CommandLine> GetOsUpdateHandlerCommand(
    const InstallerState& installer_state,
    const std::wstring& installed_version,
    const base::CommandLine& command_line) {
  const auto args = command_line.GetArgs();
  if (args.size() != 1) {
    return std::nullopt;
  }
  // Use the Windows version update string set by Omaha on the command line
  // as the version update string to pass to os_update_handler.exe.
  base::CommandLine os_update_handler_cmd(installer_state.target_path()
                                              .Append(installed_version)
                                              .Append(kOsUpdateHandlerExe));
  InstallUtil::AppendModeAndChannelSwitches(&os_update_handler_cmd);
  // args[0] has the form "<prev_windows_version>-<new_windows_version>".
  os_update_handler_cmd.AppendArgNative(args[0]);

  if (installer_state.system_install()) {
    os_update_handler_cmd.AppendSwitch(installer::switches::kSystemLevel);
  }
  return os_update_handler_cmd;
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void CreateOrUpdateShortcuts(const base::FilePath& target,
                             const InitialPreferences& prefs,
                             InstallShortcutLevel install_level,
                             InstallShortcutOperation install_operation) {
  bool do_not_create_any_shortcuts = false;
  prefs.GetBool(initial_preferences::kDoNotCreateAnyShortcuts,
                &do_not_create_any_shortcuts);
  if (do_not_create_any_shortcuts)
    return;

  // Extract shortcut preferences from |prefs|.
  bool do_not_create_desktop_shortcut = false;
  bool do_not_create_quick_launch_shortcut = false;
  bool do_not_create_taskbar_shortcut = false;
  prefs.GetBool(initial_preferences::kDoNotCreateDesktopShortcut,
                &do_not_create_desktop_shortcut);
  prefs.GetBool(initial_preferences::kDoNotCreateQuickLaunchShortcut,
                &do_not_create_quick_launch_shortcut);
  prefs.GetBool(initial_preferences::kDoNotCreateTaskbarShortcut,
                &do_not_create_taskbar_shortcut);

  // Pinning to taskbar only makes sense for per-user shortcuts.
  if (install_level != CURRENT_USER) {
    do_not_create_taskbar_shortcut = true;
  }

  // The default operation on update is to overwrite shortcuts with the
  // currently desired properties, but do so only for shortcuts that still
  // exist.
  ShellUtil::ShortcutOperation shortcut_operation;
  switch (install_operation) {
    case INSTALL_SHORTCUT_CREATE_ALL:
      shortcut_operation = ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS;
      break;
    case INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL:
      shortcut_operation = ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL;
      break;
    default:
      DCHECK(install_operation == INSTALL_SHORTCUT_REPLACE_EXISTING);
      shortcut_operation = ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING;
      break;
  }

  // Shortcuts are always installed per-user unless specified.
  ShellUtil::ShellChange shortcut_level =
      (install_level == ALL_USERS ? ShellUtil::SYSTEM_LEVEL
                                  : ShellUtil::CURRENT_USER);

  // |base_properties|: The basic properties to set on every shortcut installed
  // (to be refined on a per-shortcut basis).
  ShellUtil::ShortcutProperties base_properties(shortcut_level);
  ShellUtil::AddDefaultShortcutProperties(target, &base_properties);

  if (!do_not_create_desktop_shortcut ||
      shortcut_operation == ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING) {
    ExecuteAndLogShortcutOperation(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                   base_properties, shortcut_operation);
  }

  if (!do_not_create_quick_launch_shortcut ||
      shortcut_operation == ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING) {
    // There is no such thing as an all-users Quick Launch shortcut, always
    // install the per-user shortcut.
    ShellUtil::ShortcutProperties quick_launch_properties(base_properties);
    quick_launch_properties.level = ShellUtil::CURRENT_USER;
    ExecuteAndLogShortcutOperation(ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH,
                                   quick_launch_properties, shortcut_operation);
  }

  ShellUtil::ShortcutProperties start_menu_properties(base_properties);
  if (shortcut_operation == ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS ||
      shortcut_operation ==
          ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL) {
    start_menu_properties.set_pin_to_taskbar(!do_not_create_taskbar_shortcut);
  }

  const CLSID toast_activator_clsid = install_static::GetToastActivatorClsid();
  if (toast_activator_clsid != CLSID_NULL)
    start_menu_properties.set_toast_activator_clsid(toast_activator_clsid);

  // The attempt below to update the stortcut will fail if it does not already
  // exist at the expected location on disk.  First check if it exists in the
  // previous location (under a subdirectory) and, if so, move it to the new
  // location.
  base::FilePath old_shortcut_path;
  if (!ShellUtil::GetShortcutPath(
          ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
          shortcut_level, &old_shortcut_path)) {
    return;
  }
  if (base::PathExists(old_shortcut_path)) {
    ShellUtil::MoveExistingShortcut(
        ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED,
        ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, start_menu_properties);
  }

  ExecuteAndLogShortcutOperation(ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT,
                                 start_menu_properties, shortcut_operation);
}

// Registers Chrome on this machine.
// If |make_chrome_default|, also attempts to make Chrome default where doing so
// requires no more user interaction than a UAC prompt. In practice, this means
// on versions of Windows prior to Windows 8.
// |version| the current version of this install.
void RegisterChromeOnMachine(const InstallerState& installer_state,
                             bool make_chrome_default,
                             const base::Version& version) {
  // Try to add Chrome to Media Player shim inclusion list. We don't do any
  // error checking here because this operation will fail if user doesn't
  // have admin rights and we want to ignore the error.
  AddChromeToMediaPlayerList();

  // Register the event log provider for system-level installs only, as it
  // requires admin privileges.
  if (installer_state.system_install())
    RegisterEventLogProvider(installer_state.target_path(), version);

  // Make Chrome the default browser if desired when possible. Otherwise, only
  // register it with Windows.
  const base::FilePath chrome_exe(
      installer_state.target_path().Append(kChromeExe));
  VLOG(1) << "Registering Chrome as browser: " << chrome_exe.value();
  if (make_chrome_default && install_static::SupportsSetAsDefaultBrowser() &&
      ShellUtil::CanMakeChromeDefaultUnattended()) {
    int level = ShellUtil::CURRENT_USER;
    if (installer_state.system_install())
      level = level | ShellUtil::SYSTEM_LEVEL;
    ShellUtil::MakeChromeDefault(level, chrome_exe, true);
  } else {
    ShellUtil::RegisterChromeBrowserBestEffort(chrome_exe);
  }
}

// Run a child process that will create/update a shortcut for an
// install. This is done in a child process to avoid crashing the main
// install process if we crash in Windows shell functions. For more info,
// see crbug.com/1276348.
void RunShortcutCreationInChildProc(
    const InstallerState& installer_state,
    const base::FilePath& setup_path,
    const std::optional<const base::FilePath>& prefs_path,
    InstallShortcutLevel install_level,
    InstallShortcutOperation install_operation) {
  base::CommandLine command_line(setup_path);
  InstallUtil::AppendModeAndChannelSwitches(&command_line);
  if (installer_state.system_install())
    command_line.AppendSwitch(switches::kSystemLevel);

  command_line.AppendSwitch(switches::kVerboseLogging);
  if (prefs_path.has_value())
    command_line.AppendSwitchPath(switches::kInstallerData, prefs_path.value());

  command_line.AppendSwitchASCII(switches::kCreateShortcuts,
                                 base::NumberToString(install_operation));
  command_line.AppendSwitchASCII(switches::kInstallLevel,
                                 base::NumberToString(install_level));
  base::LaunchOptions launch_options;
  launch_options.feedback_cursor_off = true;

  VLOG(1) << "Launching \"" << command_line.GetCommandLineString()
          << "\" to create shortcuts";
  ::SetLastError(ERROR_SUCCESS);
  base::Process process = base::LaunchProcess(command_line, launch_options);
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch \"" << command_line.GetCommandLineString()
                << "\"";
    return;
  }
  int exit_code = OS_ERROR;
  process.Process::WaitForExit(&exit_code);

  if (exit_code != CREATE_SHORTCUTS_SUCCESS) {
    LOG(ERROR) << "Launch shortcut creation process failed with exit code "
               << exit_code;
  } else {
    VLOG(1) << "Shortcut creation process succeeded.";
  }
}

InstallStatus InstallOrUpdateProduct(const InstallParams& install_params,
                                     const base::FilePath& prefs_path,
                                     const InitialPreferences& prefs) {
  const InstallationState& original_state = *install_params.installation_state;
  const InstallerState& installer_state = *install_params.installer_state;
  const base::FilePath& setup_path = *install_params.setup_path;
  const base::FilePath& src_path = *install_params.src_path;
  const base::Version& new_version = *install_params.new_version;

  // TODO(robertshield): Removing the pending on-reboot moves should be done
  // elsewhere.
  // Remove any scheduled MOVEFILE_DELAY_UNTIL_REBOOT entries in the target of
  // this installation. These may have been added during a previous uninstall of
  // the same version.
  LOG_IF(ERROR, !RemoveFromMovesPendingReboot(installer_state.target_path()))
      << "Error accessing pending moves value.";

  // Create VisualElementManifest.xml in |src_path| (if required) so that it
  // looks as if it had been extracted from the archive when calling
  // InstallNewVersion() below.
  installer_state.SetStage(CREATING_VISUAL_MANIFEST);
  CreateVisualElementsManifest(src_path, new_version);

  InstallStatus result =
      InstallNewVersion(install_params, IsDowngradeAllowed(prefs));

  // TODO(robertshield): Everything below this line should instead be captured
  // by WorkItems.
  if (!InstallUtil::GetInstallReturnCode(result)) {
    installer_state.SetStage(COPYING_PREFERENCES_FILE);

    const bool use_initial_prefs =
        result == FIRST_INSTALL_SUCCESS && !prefs_path.empty();
    if (use_initial_prefs)
      CopyPreferenceFileForFirstRun(installer_state, prefs_path);

    installer_state.SetStage(CREATING_SHORTCUTS);
    InstallShortcutOperation install_operation =
        INSTALL_SHORTCUT_REPLACE_EXISTING;
    if (result == FIRST_INSTALL_SUCCESS ||
        !original_state.GetProductState(installer_state.system_install())) {
      // Always create the shortcuts on a new install and when the Chrome
      // product is being added to the current install.
      install_operation = INSTALL_SHORTCUT_CREATE_ALL;
    } else if (result == INSTALL_REPAIRED &&
               InstallUtil::IsRunningAsInteractiveUser()) {
      // If the install was a user initiated repair, create the shortcuts.
      VLOG(1) << "User initiated repair, will create shortcuts.";
      install_operation = INSTALL_SHORTCUT_CREATE_ALL;
    }
    InstallShortcutLevel install_level =
        installer_state.system_install() ? ALL_USERS : CURRENT_USER;
    RunShortcutCreationInChildProc(
        installer_state, setup_path,
        use_initial_prefs ? std::optional<base::FilePath>(prefs_path)
                          : std::nullopt,
        install_level, install_operation);

    // Register Chrome and, if requested, make Chrome the default browser.
    installer_state.SetStage(REGISTERING_CHROME);

    bool make_chrome_default = false;
    prefs.GetBool(initial_preferences::kMakeChromeDefault,
                  &make_chrome_default);

    // If this is not the user's first Chrome install, but they have chosen
    // Chrome to become their default browser on the download page, we must
    // force it here because the initial preferences file will not get copied
    // into the build.
    bool force_chrome_default_for_user = false;
    if (result == NEW_VERSION_UPDATED || result == INSTALL_REPAIRED ||
        result == OLD_VERSION_DOWNGRADE || result == IN_USE_DOWNGRADE) {
      prefs.GetBool(initial_preferences::kMakeChromeDefaultForUser,
                    &force_chrome_default_for_user);
    }

    RegisterChromeOnMachine(
        installer_state, make_chrome_default || force_chrome_default_for_user,
        new_version);

    if (!installer_state.system_install()) {
      UpdateDefaultBrowserBeaconForPath(
          installer_state.target_path().Append(kChromeExe));
    }

    // Delete files that belong to old versions of Chrome. If that fails during
    // a not-in-use update, launch a --delete-old-version process. If this is an
    // in-use update, a --delete-old-versions process will be launched when
    // executables are renamed.
    installer_state.SetStage(REMOVING_OLD_VERSIONS);
    const bool is_in_use =
        (result == IN_USE_UPDATED || result == IN_USE_DOWNGRADE);
    if (!DeleteOldVersions(installer_state.target_path()) && !is_in_use) {
      const base::FilePath new_version_setup_path =
          installer_state.GetInstallerDirectory(new_version)
              .Append(setup_path.BaseName());
      LaunchDeleteOldVersionsProcess(new_version_setup_path, installer_state);
    }
  }

  return result;
}

void LaunchDeleteOldVersionsProcess(const base::FilePath& setup_path,
                                    const InstallerState& installer_state) {
  base::CommandLine command_line(setup_path);
  InstallUtil::AppendModeAndChannelSwitches(&command_line);
  command_line.AppendSwitch(switches::kDeleteOldVersions);

  if (installer_state.system_install())
    command_line.AppendSwitch(switches::kSystemLevel);
  // Unconditionally enable verbose logging for now to make diagnosing potential
  // failures possible.
  command_line.AppendSwitch(switches::kVerboseLogging);

  base::LaunchOptions launch_options;
  launch_options.start_hidden = true;
  // Make sure not to launch from a version directory. Otherwise, it wouldn't be
  // possible to delete it.
  launch_options.current_directory = setup_path.DirName();
  launch_options.force_breakaway_from_job_ = true;

  VLOG(1) << "Launching \"" << command_line.GetCommandLineString()
          << "\" to delete old versions.";
  base::Process process = base::LaunchProcess(command_line, launch_options);
  PLOG_IF(ERROR, !process.IsValid())
      << "Failed to launch \"" << command_line.GetCommandLineString() << "\"";
}

void HandleOsUpgradeForBrowser(const InstallerState& installer_state,
                               const base::Version& installed_version,
                               const base::FilePath& setup_path) {
  VLOG(1) << "Updating and registering shortcuts for --on-os-upgrade.";

  // Update shortcuts at this install level (per-user shortcuts on system-level
  // installs will be updated through Active Setup).
  const InstallShortcutLevel level =
      installer_state.system_install() ? ALL_USERS : CURRENT_USER;

  RunShortcutCreationInChildProc(
      installer_state, setup_path,
      InitialPreferences::Path(installer_state.target_path()), level,
      INSTALL_SHORTCUT_REPLACE_EXISTING);

  // Adapt Chrome registrations to this new OS.
  RegisterChromeOnMachine(installer_state, false, installed_version);

  // Active Setup registrations are sometimes lost across OS update, make sure
  // they're back in place. Note: when Active Setup registrations in HKLM are
  // lost, the per-user values of performed Active Setups in HKCU are also lost,
  // so it is fine to restart the dynamic components of the Active Setup version
  // (ref. UpdateActiveSetupVersionWorkItem) from scratch.
  // TODO(gab): This should really perform all registry only update steps (i.e.,
  // something between InstallOrUpdateProduct and AddActiveSetupWorkItems, but
  // this takes care of what is most required for now).
  std::unique_ptr<WorkItemList> work_item_list(WorkItem::CreateWorkItemList());
  AddActiveSetupWorkItems(installer_state, installed_version,
                          work_item_list.get());
  if (!work_item_list->Do()) {
    LOG(WARNING) << "Failed to reinstall Active Setup keys.";
    work_item_list->Rollback();
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  LaunchOSUpdateHandlerIfNeeded(
      installer_state, base::ASCIIToWide(installed_version.GetString()));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  UpdateOsUpgradeBeacon();

  // Update the per-user default browser beacon. For user-level installs this
  // can be done directly; whereas it requires triggering Active Setup for each
  // user's subsequent login on system-level installs.
  if (!installer_state.system_install()) {
    UpdateDefaultBrowserBeaconForPath(
        installer_state.target_path().Append(kChromeExe));
  } else {
    UpdateActiveSetupVersionWorkItem active_setup_work_item(
        install_static::GetActiveSetupPath(),
        UpdateActiveSetupVersionWorkItem::UPDATE_AND_BUMP_SELECTIVE_TRIGGER);
    if (active_setup_work_item.Do())
      VLOG(1) << "Bumped Active Setup Version on-os-upgrade.";
    else
      LOG(ERROR) << "Failed to bump Active Setup Version on-os-upgrade.";
  }
}

// NOTE: Should the work done here, on Active Setup, change:
// kActiveSetupMajorVersion in update_active_setup_version_work_item.cc needs to
// be increased for Active Setup to invoke this again for all users of this
// install. It may also be invoked again when a system-level chrome install goes
// through an OS upgrade.
void HandleActiveSetupForBrowser(const InstallerState& installer_state,
                                 const base::FilePath& setup_path,
                                 bool force) {
  std::unique_ptr<WorkItemList> cleanup_list(WorkItem::CreateWorkItemList());
  cleanup_list->set_log_message("Cleanup deprecated per-user registrations");
  cleanup_list->set_rollback_enabled(false);
  cleanup_list->set_best_effort(true);
  AddCleanupDeprecatedPerUserRegistrationsWorkItems(cleanup_list.get());
  cleanup_list->Do();

  // Only create shortcuts on Active Setup if the first run sentinel is not
  // present for this user (as some shortcuts used to be installed on first
  // run and this could otherwise re-install shortcuts for users that have
  // already deleted them in the past).
  // Decide whether to create the shortcuts or simply replace existing
  // shortcuts; if the decision is to create them, only shortcuts whose matching
  // all-users shortcut isn't present on the system will be created.
  InstallShortcutOperation install_operation =
      (!force && InstallUtil::IsFirstRunSentinelPresent())
          ? INSTALL_SHORTCUT_REPLACE_EXISTING
          : INSTALL_SHORTCUT_CREATE_EACH_IF_NO_SYSTEM_LEVEL;

  // Use the initial preferences copied beside chrome.exe at install for the
  // sake of creating/updating shortcuts.
  const base::FilePath installation_root = installer_state.target_path();
  RunShortcutCreationInChildProc(installer_state, setup_path,
                                 InitialPreferences::Path(installation_root),
                                 CURRENT_USER, install_operation);

  UpdateDefaultBrowserBeaconForPath(installation_root.Append(kChromeExe));
}

}  // namespace installer
