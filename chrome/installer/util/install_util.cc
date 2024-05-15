// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// See the corresponding header file for description of the functions in this
// file.

#include "chrome/installer/util/install_util.h"

#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <iterator>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "base/win/shlwapi.h"
#include "base/win/shortcut.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

using base::win::RegKey;
using installer::ProductState;

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class StartMenuShortcutStatus {
  kSuccess = 0,
  kGetShortcutPathFailed = 1,
  kShortcutMissing = 2,
  kToastActivatorClsidIncorrect = 3,
  kReadShortcutPropertyFailed = 4,
  kMaxValue = kReadShortcutPropertyFailed,
};

void LogStartMenuShortcutStatus(StartMenuShortcutStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Notifications.Windows.StartMenuShortcutStatus",
                            status);
}

// Creates a zero-sized non-decorated foreground window that doesn't appear
// in the taskbar. This is used as a parent window for calls to ShellExecuteEx
// in order for the UAC dialog to appear in the foreground and for focus
// to be returned to this process once the UAC task is dismissed. Returns
// nullptr on failure, a handle to the UAC window on success.
HWND CreateUACForegroundWindow() {
  HWND foreground_window = ::CreateWindowEx(
      WS_EX_TOOLWINDOW, L"STATIC", nullptr, WS_POPUP | WS_VISIBLE, 0, 0, 0, 0,
      nullptr, nullptr, ::GetModuleHandle(nullptr), nullptr);
  if (foreground_window) {
    HMONITOR monitor =
        ::MonitorFromWindow(foreground_window, MONITOR_DEFAULTTONEAREST);
    if (monitor) {
      MONITORINFO mi = {0};
      mi.cbSize = sizeof(mi);
      ::GetMonitorInfo(monitor, &mi);
      RECT screen_rect = mi.rcWork;
      int x_offset = (screen_rect.right - screen_rect.left) / 2;
      int y_offset = (screen_rect.bottom - screen_rect.top) / 2;
      ::MoveWindow(foreground_window, screen_rect.left + x_offset,
                   screen_rect.top + y_offset, 0, 0, FALSE);
    } else {
      NOTREACHED_IN_MIGRATION() << "Unable to get default monitor";
    }
    ::SetForegroundWindow(foreground_window);
  }
  return foreground_window;
}

// Returns Registry key path of Chrome policies. This is used by the policies
// that are shared between Chrome and installer.
std::wstring GetChromePoliciesRegistryPath() {
  std::wstring key_path = L"SOFTWARE\\Policies\\";
  install_static::AppendChromeInstallSubDirectory(
      install_static::InstallDetails::Get().mode(), /*include_suffix=*/false,
      &key_path);
  return key_path;
}

std::wstring GetCloudManagementPoliciesRegistryPath() {
  std::wstring key_path = L"SOFTWARE\\Policies\\";
  key_path.append(install_static::kCompanyPathName);
  key_path.append(L"\\CloudManagement");
  return key_path;
}

// Reruns the registry key path and value name where the cloud management
// enrollment option is stored.
void GetCloudManagementBlockOnFailureRegistryPath(std::wstring* key_path,
                                                  std::wstring* value_name) {
  *key_path = GetChromePoliciesRegistryPath();
  *value_name = L"CloudManagementEnrollmentMandatory";
}

}  // namespace

void InstallUtil::TriggerActiveSetupCommand() {
  std::wstring active_setup_reg(install_static::GetActiveSetupPath());
  base::win::RegKey active_setup_key(HKEY_LOCAL_MACHINE,
                                     active_setup_reg.c_str(), KEY_QUERY_VALUE);
  std::wstring cmd_str;
  LONG read_status = active_setup_key.ReadValue(L"StubPath", &cmd_str);
  if (read_status != ERROR_SUCCESS) {
    LOG(ERROR) << active_setup_reg << ", " << read_status;
    // This should never fail if Chrome is registered at system-level, but if it
    // does there is not much else to be done.
    return;
  }

  base::CommandLine cmd(base::CommandLine::FromString(cmd_str));
  // Force creation of shortcuts as the First Run beacon might land between now
  // and the time setup.exe checks for it.
  cmd.AppendSwitch(installer::switches::kForceConfigureUserSettings);

  base::Process process =
      base::LaunchProcess(cmd.GetCommandLineString(), base::LaunchOptions());
  if (!process.IsValid())
    PLOG(ERROR) << cmd.GetCommandLineString();
}

bool InstallUtil::ExecuteExeAsAdmin(const base::CommandLine& cmd,
                                    DWORD* exit_code) {
  base::FilePath::StringType program(cmd.GetProgram().value());
  DCHECK(!program.empty());
  DCHECK_NE(program[0], L'\"');

  base::CommandLine::StringType params(cmd.GetCommandLineString());
  if (params[0] == '"') {
    DCHECK_EQ('"', params[program.length() + 1]);
    DCHECK_EQ(program, params.substr(1, program.length()));
    params = params.substr(program.length() + 2);
  } else {
    DCHECK_EQ(program, params.substr(0, program.length()));
    params = params.substr(program.length());
  }

  base::TrimWhitespace(params, base::TRIM_ALL, &params);

  HWND uac_foreground_window = CreateUACForegroundWindow();

  SHELLEXECUTEINFO info = {0};
  info.cbSize = sizeof(SHELLEXECUTEINFO);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.hwnd = uac_foreground_window;
  info.lpVerb = L"runas";
  info.lpFile = program.c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_SHOW;

  bool success = false;
  if (::ShellExecuteEx(&info) == TRUE) {
    ::WaitForSingleObject(info.hProcess, INFINITE);
    DWORD ret_val = 0;
    if (::GetExitCodeProcess(info.hProcess, &ret_val)) {
      success = true;
      if (exit_code)
        *exit_code = ret_val;
    }
  }

  if (uac_foreground_window) {
    DestroyWindow(uac_foreground_window);
  }

  return success;
}

base::CommandLine InstallUtil::GetChromeUninstallCmd(bool system_install) {
  ProductState state;
  if (state.Initialize(system_install))
    return state.uninstall_command();
  return base::CommandLine(base::CommandLine::NO_PROGRAM);
}

base::Version InstallUtil::GetChromeVersion(bool system_install) {
  base::Version version;
  RegKey key;
  std::wstring version_str;
  if (key.Open(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
               install_static::GetClientsKeyPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(google_update::kRegVersionField, &version_str) ==
          ERROR_SUCCESS &&
      !version_str.empty()) {
    version = base::Version(base::WideToASCII(version_str));
  }

  if (version.IsValid())
    VLOG(1) << "Existing Chrome version found: " << version.GetString();
  else
    VLOG(1) << "No existing Chrome install found.";

  return version;
}

base::Version InstallUtil::GetCriticalUpdateVersion() {
  base::Version version;
  RegKey key;
  std::wstring version_str;
  if (key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                 : HKEY_CURRENT_USER,
               install_static::GetClientsKeyPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(google_update::kRegCriticalVersionField, &version_str) ==
          ERROR_SUCCESS &&
      !version_str.empty()) {
    version = base::Version(base::WideToASCII(version_str));
  }

  if (version.IsValid())
    VLOG(1) << "Critical Update version found: " << version.GetString();
  else
    VLOG(1) << "No existing Chrome install found.";

  return version;
}

bool InstallUtil::IsOSSupported() {
  // We do not support anything prior to Windows 10.
  VLOG(1) << base::SysInfo::OperatingSystemName() << ' '
          << base::SysInfo::OperatingSystemVersion();
  return base::win::GetVersion() >= base::win::Version::WIN10;
}

void InstallUtil::AddInstallerResultItems(bool system_install,
                                          const std::wstring& state_key,
                                          installer::InstallStatus status,
                                          int string_resource_id,
                                          const std::wstring* const launch_cmd,
                                          WorkItemList* install_list) {
  DCHECK(install_list);
  DCHECK(install_list->best_effort());
  DCHECK(!install_list->rollback_enabled());

  const HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  DWORD installer_result = (GetInstallReturnCode(status) == 0) ? 0 : 1;
  install_list->AddCreateRegKeyWorkItem(root, state_key, KEY_WOW64_32KEY);
  install_list->AddSetRegValueWorkItem(root, state_key, KEY_WOW64_32KEY,
                                       installer::kInstallerResult,
                                       installer_result, true);
  install_list->AddSetRegValueWorkItem(root, state_key, KEY_WOW64_32KEY,
                                       installer::kInstallerError,
                                       static_cast<DWORD>(status), true);
  if (string_resource_id != 0) {
    std::wstring msg = installer::GetLocalizedString(string_resource_id);
    install_list->AddSetRegValueWorkItem(root, state_key, KEY_WOW64_32KEY,
                                         installer::kInstallerResultUIString,
                                         msg, true);
  }
  if (launch_cmd != nullptr && !launch_cmd->empty()) {
    install_list->AddSetRegValueWorkItem(
        root, state_key, KEY_WOW64_32KEY,
        installer::kInstallerSuccessLaunchCmdLine, *launch_cmd, true);
  }
}

bool InstallUtil::IsPerUserInstall() {
  return !install_static::InstallDetails::Get().system_level();
}

// static
bool InstallUtil::IsFirstRunSentinelPresent() {
  // TODO(msw): Consolidate with first_run::internal::IsFirstRunSentinelPresent.
  base::FilePath user_data_dir;
  return !base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) ||
         base::PathExists(user_data_dir.Append(chrome::kFirstRunSentinel));
}

// static
bool InstallUtil::IsStartMenuShortcutWithActivatorGuidInstalled() {
  base::FilePath shortcut_path;

  if (!ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT,
                                  install_static::IsSystemInstall()
                                      ? ShellUtil::SYSTEM_LEVEL
                                      : ShellUtil::CURRENT_USER,
                                  &shortcut_path)) {
    LogStartMenuShortcutStatus(StartMenuShortcutStatus::kGetShortcutPathFailed);
    return false;
  }

  shortcut_path = shortcut_path.Append(GetShortcutName() + installer::kLnkExt);
  if (!base::PathExists(shortcut_path)) {
    LogStartMenuShortcutStatus(StartMenuShortcutStatus::kShortcutMissing);
    return false;
  }

  base::win::ShortcutProperties properties;
  if (!base::win::ResolveShortcutProperties(
          shortcut_path,
          base::win::ShortcutProperties::PROPERTIES_TOAST_ACTIVATOR_CLSID,
          &properties)) {
    LogStartMenuShortcutStatus(
        StartMenuShortcutStatus::kReadShortcutPropertyFailed);
    return false;
  }

  if (!::IsEqualCLSID(properties.toast_activator_clsid,
                      install_static::GetToastActivatorClsid())) {
    LogStartMenuShortcutStatus(
        StartMenuShortcutStatus::kToastActivatorClsidIncorrect);

    return false;
  }

  LogStartMenuShortcutStatus(StartMenuShortcutStatus::kSuccess);
  return true;
}

// static
bool InstallUtil::IsRunningAsInteractiveUser() {
  // Get the SID for interactive user.
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  uint8_t sid_bytes[SECURITY_MAX_SID_SIZE] = {0};
  SID* interactive_sid = reinterpret_cast<SID*>(sid_bytes);
  if (!::CreateWellKnownSid(WinInteractiveSid, nullptr, interactive_sid,
                            &sid_size)) {
    PLOG(ERROR) << "Failed to create well known SID";
    return false;
  }

  BOOL is_member = FALSE;
  if (!::CheckTokenMembership(nullptr, interactive_sid, &is_member)) {
    PLOG(ERROR) << "Failed to check token membership for WinInteractiveSid";
    return false;
  }

  return is_member;
}

// static
std::wstring InstallUtil::GetToastActivatorRegistryPath() {
  return L"Software\\Classes\\CLSID\\" +
         base::win::WStringFromGUID(install_static::GetToastActivatorClsid());
}

// static
bool InstallUtil::GetEulaSentinelFilePath(base::FilePath* path) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return false;
  *path = user_data_dir.Append(installer::kEulaSentinelFile);
  return true;
}

// static
int InstallUtil::GetInstallReturnCode(installer::InstallStatus status) {
  switch (status) {
    case installer::FIRST_INSTALL_SUCCESS:
    case installer::INSTALL_REPAIRED:
    case installer::NEW_VERSION_UPDATED:
    case installer::IN_USE_UPDATED:
    case installer::OLD_VERSION_DOWNGRADE:
    case installer::IN_USE_DOWNGRADE:
      return 0;
    default:
      return status;
  }
}

// static
void InstallUtil::ComposeCommandLine(const std::wstring& program,
                                     const std::wstring& arguments,
                                     base::CommandLine* command_line) {
  *command_line =
      base::CommandLine::FromString(L"\"" + program + L"\" " + arguments);
}

void InstallUtil::AppendModeAndChannelSwitches(
    base::CommandLine* command_line) {
  const install_static::InstallDetails& install_details =
      install_static::InstallDetails::Get();
  if (*install_details.install_switch())
    command_line->AppendSwitch(install_details.install_switch());
  if (install_details.channel_origin() ==
      install_static::ChannelOrigin::kPolicy) {
    // Use channel_override rather than simply channel so that extended stable
    // is differentiated from regular.
    command_line->AppendSwitchNative(installer::switches::kChannel,
                                     install_details.channel_override());
  }
}

// static
std::wstring InstallUtil::GetCurrentDate() {
  static const wchar_t kDateFormat[] = L"yyyyMMdd";
  wchar_t date_str[std::size(kDateFormat)] = {0};
  int len = GetDateFormatW(LOCALE_INVARIANT, 0, nullptr, kDateFormat, date_str,
                           std::size(date_str));
  if (len) {
    --len;  // Subtract terminating \0.
  } else {
    PLOG(DFATAL) << "GetDateFormat";
  }

  return std::wstring(date_str, len);
}

// static
std::optional<base::Version> InstallUtil::GetDowngradeVersion() {
  RegKey key;
  std::wstring downgrade_version;
  if (key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                 : HKEY_CURRENT_USER,
               install_static::GetClientStateKeyPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) != ERROR_SUCCESS ||
      key.ReadValue(installer::kRegDowngradeVersion, &downgrade_version) !=
          ERROR_SUCCESS ||
      downgrade_version.empty()) {
    return std::nullopt;
  }
  base::Version version(base::WideToASCII(downgrade_version));
  if (!version.IsValid())
    return std::nullopt;
  return version;
}

// static
std::vector<std::pair<std::wstring, std::wstring>>
InstallUtil::GetCloudManagementEnrollmentTokenRegistryPaths() {
  std::vector<std::pair<std::wstring, std::wstring>> paths;
  // Prefer the product-agnostic location used by Google Update.
  paths.emplace_back(GetCloudManagementPoliciesRegistryPath(),
                     L"EnrollmentToken");
  // Follow that with the Google Chrome policy.
  paths.emplace_back(GetChromePoliciesRegistryPath(),
                     L"CloudManagementEnrollmentToken");
  return paths;
}

// static
std::pair<std::wstring, std::wstring>
InstallUtil::GetCloudManagementDmTokenPath(BrowserLocation browser_location) {
  std::wstring key_path = L"SOFTWARE\\";
  if (browser_location) {
    install_static::AppendChromeInstallSubDirectory(
        install_static::InstallDetails::Get().mode(), /*include_suffix=*/false,
        &key_path);
  } else {
    key_path.append(install_static::kCompanyPathName);
  }
  key_path.append(L"\\Enrollment");

  return {key_path, L"dmtoken"};
}

// static
std::pair<base::win::RegKey, std::wstring>
InstallUtil::GetCloudManagementDmTokenLocation(
    ReadOnly read_only,
    BrowserLocation browser_location) {
  // The location dictates the WoW bit.
  REGSAM wow_access = browser_location ? KEY_WOW64_64KEY : KEY_WOW64_32KEY;
  auto [key_path, value_name] = GetCloudManagementDmTokenPath(browser_location);

  base::win::RegKey key;
  if (read_only) {
    (void)key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(),
                   KEY_QUERY_VALUE | wow_access);
  } else {
    auto result = key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(),
                             KEY_SET_VALUE | wow_access);
    if (result != ERROR_SUCCESS) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed to create/open registry key HKLM\\" << key_path
                  << " for writing";
    }
  }

  return {std::move(key), value_name};
}

// static
std::tuple<base::win::RegKey, std::wstring, std::wstring>
InstallUtil::GetDeviceTrustSigningKeyLocation(ReadOnly read_only) {
  // The location dictates the path and WoW bit.
  std::wstring key_path = L"SOFTWARE\\";
  install_static::AppendChromeInstallSubDirectory(
      install_static::InstallDetails::Get().mode(), /*include_suffix=*/false,
      &key_path)
      .append(L"\\DeviceTrust");
  base::win::RegKey key;
  if (read_only) {
    (void)key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(),
                   KEY_QUERY_VALUE | KEY_WOW64_64KEY);
  } else {
    auto result = key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(),
                             KEY_SET_VALUE | KEY_WOW64_64KEY);
    if (result != ERROR_SUCCESS) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed to create/open registry key HKLM\\" << key_path
                  << " for writing";
    }
  }

  return {std::move(key), L"signing_key", L"trust_level"};
}

// static
std::wstring InstallUtil::GetCloudManagementEnrollmentToken() {
  // Because chrome needs to know if machine level user cloud policies must be
  // initialized even before the entire policy service is brought up, this
  // helper function exists to directly read the token from the system policies.
  //
  // Putting the enrollment token in the system policy area is a convenient
  // way for administrators to enroll chrome throughout their fleet by pushing
  // this token via SCCM.
  // TODO(rogerta): This may not be the best place for the helpers dealing with
  // the enrollment and/or DM tokens.  See crbug.com/823852 for details.
  RegKey key;
  std::wstring value;
  for (const auto& key_and_value :
       GetCloudManagementEnrollmentTokenRegistryPaths()) {
    if (key.Open(HKEY_LOCAL_MACHINE, key_and_value.first.c_str(),
                 KEY_QUERY_VALUE) == ERROR_SUCCESS &&
        key.ReadValue(key_and_value.second.c_str(), &value) == ERROR_SUCCESS) {
      return value;
    }
  }

  return std::wstring();
}

// static
bool InstallUtil::ShouldCloudManagementBlockOnFailure() {
  std::wstring key_path;
  std::wstring value_name;
  GetCloudManagementBlockOnFailureRegistryPath(&key_path, &value_name);

  DWORD value = 0;
  RegKey(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_QUERY_VALUE)
      .ReadValueDW(value_name.c_str(), &value);

  return value != 0;
}

// static
std::wstring InstallUtil::GetDisplayName() {
  return GetShortcutName();
}

// static
std::wstring InstallUtil::GetAppDescription() {
  return installer::GetLocalizedString(IDS_SHORTCUT_TOOLTIP_BASE);
}

// static
std::wstring InstallUtil::GetPublisherName() {
  return installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
}

// static
std::wstring InstallUtil::GetShortcutName() {
  // IDS_PRODUCT_NAME is automatically mapped to the mode-specific shortcut
  // name; see MODE_SPECIFIC_STRINGS in prebuild/create_string_rc.py.
  return installer::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
}

// static
std::wstring InstallUtil::GetChromeShortcutDirNameDeprecated() {
  return GetShortcutName();
}

// static
std::wstring InstallUtil::GetChromeAppsShortcutDirName() {
  // IDS_APP_SHORTCUTS_SUBDIR_NAME is automatically mapped to the mode-specific
  // dir name; see MODE_SPECIFIC_STRINGS in prebuild/create_string_rc.py.
  return installer::GetLocalizedString(IDS_APP_SHORTCUTS_SUBDIR_NAME_BASE);
}

// static
std::wstring InstallUtil::GetLongAppDescription() {
  return installer::GetLocalizedString(IDS_PRODUCT_DESCRIPTION_BASE);
}

// static
std::wstring InstallUtil::GuidToSquid(std::wstring_view guid) {
  std::wstring squid;
  squid.reserve(32);
  auto input = guid.begin();
  auto output = std::back_inserter(squid);

  // Reverse-copy relevant characters, skipping separators.
  std::reverse_copy(input + 0, input + 8, output);
  std::reverse_copy(input + 9, input + 13, output);
  std::reverse_copy(input + 14, input + 18, output);
  std::reverse_copy(input + 19, input + 21, output);
  std::reverse_copy(input + 21, input + 23, output);
  std::reverse_copy(input + 24, input + 26, output);
  std::reverse_copy(input + 26, input + 28, output);
  std::reverse_copy(input + 28, input + 30, output);
  std::reverse_copy(input + 30, input + 32, output);
  std::reverse_copy(input + 32, input + 34, output);
  std::reverse_copy(input + 34, input + 36, output);
  return squid;
}
