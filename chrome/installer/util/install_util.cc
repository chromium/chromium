// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "base/win/registry.h"
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

// DowngradeVersion holds the version from which Chrome was downgraded. In case
// of multiple downgrades (e.g., 75->74->73), it retains the highest version
// installed prior to any downgrades. DowngradeVersion is deleted on upgrade
// once Chrome reaches the version from which it was downgraded.
const wchar_t kRegDowngradeVersion[] = L"DowngradeVersion";

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
// NULL on failure, a handle to the UAC window on success.
HWND CreateUACForegroundWindow() {
  HWND foreground_window = ::CreateWindowEx(WS_EX_TOOLWINDOW,
                                            L"STATIC",
                                            NULL,
                                            WS_POPUP | WS_VISIBLE,
                                            0, 0, 0, 0,
                                            NULL, NULL,
                                            ::GetModuleHandle(NULL),
                                            NULL);
  if (foreground_window) {
    HMONITOR monitor = ::MonitorFromWindow(foreground_window,
                                           MONITOR_DEFAULTTONEAREST);
    if (monitor) {
      MONITORINFO mi = {0};
      mi.cbSize = sizeof(mi);
      ::GetMonitorInfo(monitor, &mi);
      RECT screen_rect = mi.rcWork;
      int x_offset = (screen_rect.right - screen_rect.left) / 2;
      int y_offset = (screen_rect.bottom - screen_rect.top) / 2;
      ::MoveWindow(foreground_window,
                   screen_rect.left + x_offset,
                   screen_rect.top + y_offset,
                   0, 0, FALSE);
    } else {
      NOTREACHED() << "Unable to get default monitor";
    }
    ::SetForegroundWindow(foreground_window);
  }
  return foreground_window;
}

// Returns Registry key path of Chrome policies. This is used by the policies
// that are shared between Chrome and installer.
base::string16 GetChromePoliciesRegistryPath() {
  base::string16 key_path = L"SOFTWARE\\Policies\\";
  install_static::AppendChromeInstallSubDirectory(
      install_static::InstallDetails::Get().mode(), false /* !include_suffix */,
      &key_path);
  return key_path;
}

// Retruns the registry key path and value name where the cloud management
// enrollment option is stored.
void GetCloudManagementBlockOnFailureRegistryPath(base::string16* key_path,
                                                  base::string16* value_name) {
  *key_path = GetChromePoliciesRegistryPath();
  *value_name = L"CloudManagementEnrollmentMandatory";
}

}  // namespace

void InstallUtil::TriggerActiveSetupCommand() {
  base::string16 active_setup_reg(install_static::GetActiveSetupPath());
  base::win::RegKey active_setup_key(
      HKEY_LOCAL_MACHINE, active_setup_reg.c_str(), KEY_QUERY_VALUE);
  base::string16 cmd_str;
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
  base::string16 version_str;
  if (key.Open(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
               install_static::GetClientsKeyPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(google_update::kRegVersionField, &version_str) ==
          ERROR_SUCCESS &&
      !version_str.empty()) {
    version = base::Version(base::UTF16ToASCII(version_str));
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
  base::string16 version_str;
  if (key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                 : HKEY_CURRENT_USER,
               install_static::GetClientsKeyPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(google_update::kRegCriticalVersionField, &version_str) ==
          ERROR_SUCCESS &&
      !version_str.empty()) {
    version = base::Version(base::UTF16ToASCII(version_str));
  }

  if (version.IsValid())
    VLOG(1) << "Critical Update version found: " << version.GetString();
  else
    VLOG(1) << "No existing Chrome install found.";

  return version;
}

bool InstallUtil::IsOSSupported() {
  // We do not support anything prior to Windows 7.
  VLOG(1) << base::SysInfo::OperatingSystemName() << ' '
          << base::SysInfo::OperatingSystemVersion();
  return base::win::GetVersion() >= base::win::Version::WIN7;
}

void InstallUtil::AddInstallerResultItems(
    bool system_install,
    const base::string16& state_key,
    installer::InstallStatus status,
    int string_resource_id,
    const base::string16* const launch_cmd,
    WorkItemList* install_list) {
  DCHECK(install_list);
  DCHECK(install_list->best_effort());
  DCHECK(!install_list->rollback_enabled());

  const HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  DWORD installer_result = (GetInstallReturnCode(status) == 0) ? 0 : 1;
  install_list->AddCreateRegKeyWorkItem(root, state_key, KEY_WOW64_32KEY);
  install_list->AddSetRegValueWorkItem(root,
                                       state_key,
                                       KEY_WOW64_32KEY,
                                       installer::kInstallerResult,
                                       installer_result,
                                       true);
  install_list->AddSetRegValueWorkItem(root,
                                       state_key,
                                       KEY_WOW64_32KEY,
                                       installer::kInstallerError,
                                       static_cast<DWORD>(status),
                                       true);
  if (string_resource_id != 0) {
    base::string16 msg = installer::GetLocalizedString(string_resource_id);
    install_list->AddSetRegValueWorkItem(root,
                                         state_key,
                                         KEY_WOW64_32KEY,
                                         installer::kInstallerResultUIString,
                                         msg,
                                         true);
  }
  if (launch_cmd != NULL && !launch_cmd->empty()) {
    install_list->AddSetRegValueWorkItem(
        root,
        state_key,
        KEY_WOW64_32KEY,
        installer::kInstallerSuccessLaunchCmdLine,
        *launch_cmd,
        true);
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
base::string16 InstallUtil::GetToastActivatorRegistryPath() {
  return STRING16_LITERAL("Software\\Classes\\CLSID\\") +
         base::win::String16FromGUID(install_static::GetToastActivatorClsid());
}

// static
bool InstallUtil::GetEulaSentinelFilePath(base::FilePath* path) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return false;
  *path = user_data_dir.Append(installer::kEulaSentinelFile);
  return true;
}

// This method tries to delete a registry key and logs an error message
// in case of failure. It returns true if deletion is successful (or the key did
// not exist), otherwise false.
bool InstallUtil::DeleteRegistryKey(HKEY root_key,
                                    const base::string16& key_path,
                                    REGSAM wow64_access) {
  VLOG(1) << "Deleting registry key " << key_path;
  RegKey target_key;
  LONG result =
      target_key.Open(root_key, key_path.c_str(), DELETE | wow64_access);

  if (result == ERROR_FILE_NOT_FOUND)
    return true;

  if (result == ERROR_SUCCESS)
    result = target_key.DeleteKey(L"");

  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to delete registry key: " << key_path
               << " error: " << result;
    return false;
  }
  return true;
}

// This method tries to delete a registry value and logs an error message
// in case of failure. It returns true if deletion is successful (or the key did
// not exist), otherwise false.
bool InstallUtil::DeleteRegistryValue(HKEY reg_root,
                                      const base::string16& key_path,
                                      REGSAM wow64_access,
                                      const base::string16& value_name) {
  RegKey key;
  LONG result = key.Open(reg_root, key_path.c_str(),
                         KEY_SET_VALUE | wow64_access);
  if (result == ERROR_SUCCESS)
    result = key.DeleteValue(value_name.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
    LOG(ERROR) << "Failed to delete registry value: " << value_name
               << " error: " << result;
    return false;
  }
  return true;
}

// static
InstallUtil::ConditionalDeleteResult InstallUtil::DeleteRegistryKeyIf(
    HKEY root_key,
    const base::string16& key_to_delete_path,
    const base::string16& key_to_test_path,
    const REGSAM wow64_access,
    const wchar_t* value_name,
    const RegistryValuePredicate& predicate) {
  DCHECK(root_key);
  ConditionalDeleteResult delete_result = NOT_FOUND;
  RegKey key;
  base::string16 actual_value;
  if (key.Open(root_key, key_to_test_path.c_str(),
               KEY_QUERY_VALUE | wow64_access) == ERROR_SUCCESS &&
      key.ReadValue(value_name, &actual_value) == ERROR_SUCCESS &&
      predicate.Evaluate(actual_value)) {
    key.Close();
    delete_result = DeleteRegistryKey(root_key,
                                      key_to_delete_path,
                                      wow64_access)
        ? DELETED : DELETE_FAILED;
  }
  return delete_result;
}

// static
InstallUtil::ConditionalDeleteResult InstallUtil::DeleteRegistryValueIf(
    HKEY root_key,
    const wchar_t* key_path,
    REGSAM wow64_access,
    const wchar_t* value_name,
    const RegistryValuePredicate& predicate) {
  DCHECK(root_key);
  DCHECK(key_path);
  ConditionalDeleteResult delete_result = NOT_FOUND;
  RegKey key;
  base::string16 actual_value;
  if (key.Open(root_key, key_path,
               KEY_QUERY_VALUE | KEY_SET_VALUE | wow64_access)
          == ERROR_SUCCESS &&
      key.ReadValue(value_name, &actual_value) == ERROR_SUCCESS &&
      predicate.Evaluate(actual_value)) {
    LONG result = key.DeleteValue(value_name);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to delete registry value: "
                 << (value_name ? value_name : L"(Default)")
                 << " error: " << result;
      delete_result = DELETE_FAILED;
    } else {
      delete_result = DELETED;
    }
  }
  return delete_result;
}

bool InstallUtil::ValueEquals::Evaluate(const base::string16& value) const {
  return value == value_to_match_;
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
void InstallUtil::ComposeCommandLine(const base::string16& program,
                                     const base::string16& arguments,
                                     base::CommandLine* command_line) {
  *command_line =
      base::CommandLine::FromString(L"\"" + program + L"\" " + arguments);
}

void InstallUtil::AppendModeSwitch(base::CommandLine* command_line) {
  const install_static::InstallDetails& install_details =
      install_static::InstallDetails::Get();
  if (*install_details.install_switch())
    command_line->AppendSwitch(install_details.install_switch());
}

// static
base::string16 InstallUtil::GetCurrentDate() {
  static const wchar_t kDateFormat[] = L"yyyyMMdd";
  wchar_t date_str[base::size(kDateFormat)] = {0};
  int len = GetDateFormatW(LOCALE_INVARIANT, 0, NULL, kDateFormat, date_str,
                           base::size(date_str));
  if (len) {
    --len;  // Subtract terminating \0.
  } else {
    PLOG(DFATAL) << "GetDateFormat";
  }

  return base::string16(date_str, len);
}

// Open |path| with minimal access to obtain information about it, returning
// true and populating |file| on success.
// static
bool InstallUtil::ProgramCompare::OpenForInfo(const base::FilePath& path,
                                              base::File* file) {
  DCHECK(file);
  file->Initialize(path, base::File::FLAG_OPEN);
  return file->IsValid();
}

// Populate |info| for |file|, returning true on success.
// static
bool InstallUtil::ProgramCompare::GetInfo(const base::File& file,
                                          BY_HANDLE_FILE_INFORMATION* info) {
  DCHECK(file.IsValid());
  return GetFileInformationByHandle(file.GetPlatformFile(), info) != 0;
}

// static
base::Optional<base::Version> InstallUtil::GetDowngradeVersion() {
  RegKey key;
  base::string16 downgrade_version;
  if (key.Open(install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                                 : HKEY_CURRENT_USER,
               install_static::GetClientStateKeyPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) != ERROR_SUCCESS ||
      key.ReadValue(kRegDowngradeVersion, &downgrade_version) !=
          ERROR_SUCCESS ||
      downgrade_version.empty()) {
    return base::nullopt;
  }
  base::Version version(base::UTF16ToASCII(downgrade_version));
  if (!version.IsValid())
    return base::nullopt;
  return version;
}

// static
void InstallUtil::AddUpdateDowngradeVersionItem(
    HKEY root,
    const base::Version* current_version,
    const base::Version& new_version,
    WorkItemList* list) {
  DCHECK(list);
  const auto downgrade_version = GetDowngradeVersion();
  if (current_version && new_version < *current_version) {
    // This is a downgrade. Write the value if this is the first one (i.e., no
    // previous value exists). Otherwise, leave any existing value in place.
    if (!downgrade_version) {
      list->AddSetRegValueWorkItem(
          root, install_static::GetClientStateKeyPath(), KEY_WOW64_32KEY,
          kRegDowngradeVersion,
          base::ASCIIToUTF16(current_version->GetString()), true);
    }
  } else if (!current_version || new_version >= downgrade_version) {
    // This is a new install or an upgrade to/past a previous DowngradeVersion.
    list->AddDeleteRegValueWorkItem(root,
                                    install_static::GetClientStateKeyPath(),
                                    KEY_WOW64_32KEY, kRegDowngradeVersion);
  }
}

// static
void InstallUtil::GetMachineLevelUserCloudPolicyEnrollmentTokenRegistryPath(
    base::string16* key_path,
    base::string16* value_name,
    base::string16* old_value_name) {
  // This token applies to all installs on the machine, even though only a
  // system install can set it.  This is to prevent users from doing a user
  // install of chrome to get around policies.
  *key_path = GetChromePoliciesRegistryPath();
  *value_name = L"CloudManagementEnrollmentToken";
  *old_value_name = L"MachineLevelUserCloudPolicyEnrollmentToken";
}

// static
void InstallUtil::GetMachineLevelUserCloudPolicyDMTokenRegistryPath(
    base::string16* key_path,
    base::string16* value_name) {
  // This token applies to all installs on the machine, even though only a
  // system install can set it.  This is to prevent users from doing a user
  // install of chrome to get around policies.
  *key_path = L"SOFTWARE\\";
  install_static::AppendChromeInstallSubDirectory(
      install_static::InstallDetails::Get().mode(), false /* !include_suffix */,
      key_path);
  key_path->append(L"\\Enrollment");
  *value_name = L"dmtoken";
}

// static
base::string16 InstallUtil::GetMachineLevelUserCloudPolicyEnrollmentToken() {
  // Because chrome needs to know if machine level user cloud policies must be
  // initialized even before the entire policy service is brought up, this
  // helper function exists to directly read the token from the system policies.
  //
  // Putting the enrollment token in the system policy area is a convenient
  // way for administrators to enroll chrome throughout their fleet by pushing
  // this token via SCCM.
  // TODO(rogerta): This may not be the best place for the helpers dealing with
  // the enrollment and/or DM tokens.  See crbug.com/823852 for details.
  base::string16 key_path;
  base::string16 value_name;
  base::string16 old_value_name;
  GetMachineLevelUserCloudPolicyEnrollmentTokenRegistryPath(
      &key_path, &value_name, &old_value_name);

  base::string16 value;
  RegKey key(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_QUERY_VALUE);
  if (key.ReadValue(value_name.c_str(), &value) == ERROR_FILE_NOT_FOUND)
    key.ReadValue(old_value_name.c_str(), &value);

  return value;
}

// static
bool InstallUtil::ShouldCloudManagementBlockOnFailure() {
  base::string16 key_path;
  base::string16 value_name;
  GetCloudManagementBlockOnFailureRegistryPath(&key_path, &value_name);

  DWORD value = 0;
  RegKey(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_QUERY_VALUE)
      .ReadValueDW(value_name.c_str(), &value);

  return value != 0;
}

// static
base::string16 InstallUtil::GetDisplayName() {
  return GetShortcutName();
}

// static
base::string16 InstallUtil::GetAppDescription() {
  return installer::GetLocalizedString(IDS_SHORTCUT_TOOLTIP_BASE);
}

// static
base::string16 InstallUtil::GetPublisherName() {
  return installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
}

// static
base::string16 InstallUtil::GetShortcutName() {
  // IDS_PRODUCT_NAME is automatically mapped to the mode-specific shortcut
  // name; see MODE_SPECIFIC_STRINGS in prebuild/create_string_rc.py.
  return installer::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
}

// static
base::string16 InstallUtil::GetChromeShortcutDirNameDeprecated() {
  return GetShortcutName();
}

// static
base::string16 InstallUtil::GetChromeAppsShortcutDirName() {
  // IDS_APP_SHORTCUTS_SUBDIR_NAME is automatically mapped to the mode-specific
  // dir name; see MODE_SPECIFIC_STRINGS in prebuild/create_string_rc.py.
  return installer::GetLocalizedString(IDS_APP_SHORTCUTS_SUBDIR_NAME_BASE);
}

// static
base::string16 InstallUtil::GetLongAppDescription() {
  return installer::GetLocalizedString(IDS_PRODUCT_DESCRIPTION_BASE);
}

InstallUtil::ProgramCompare::ProgramCompare(const base::FilePath& path_to_match)
    : path_to_match_(path_to_match),
      file_info_() {
  DCHECK(!path_to_match_.empty());
  if (!OpenForInfo(path_to_match_, &file_)) {
    PLOG(WARNING) << "Failed opening " << path_to_match_.value()
                  << "; falling back to path string comparisons.";
  } else if (!GetInfo(file_, &file_info_)) {
    PLOG(WARNING) << "Failed getting information for "
                  << path_to_match_.value()
                  << "; falling back to path string comparisons.";
    file_.Close();
  }
}

InstallUtil::ProgramCompare::~ProgramCompare() {
}

bool InstallUtil::ProgramCompare::Evaluate(const base::string16& value) const {
  // Suss out the exe portion of the value, which is expected to be a command
  // line kinda (or exactly) like:
  // "c:\foo\bar\chrome.exe" -- "%1"
  base::FilePath program(base::CommandLine::FromString(value).GetProgram());
  if (program.empty()) {
    LOG(WARNING) << "Failed to parse an executable name from command line: \""
                 << value << "\"";
    return false;
  }

  return EvaluatePath(program);
}

bool InstallUtil::ProgramCompare::EvaluatePath(
    const base::FilePath& path) const {
  // Try the simple thing first: do the paths happen to match?
  if (base::FilePath::CompareEqualIgnoreCase(path_to_match_.value(),
                                             path.value()))
    return true;

  // If the paths don't match and we couldn't open the expected file, we've done
  // our best.
  if (!file_.IsValid())
    return false;

  // Open the program and see if it references the expected file.
  base::File file;
  BY_HANDLE_FILE_INFORMATION info = {};

  return (OpenForInfo(path, &file) &&
          GetInfo(file, &info) &&
          info.dwVolumeSerialNumber == file_info_.dwVolumeSerialNumber &&
          info.nFileIndexHigh == file_info_.nFileIndexHigh &&
          info.nFileIndexLow == file_info_.nFileIndexLow);
}

// static
base::string16 InstallUtil::GuidToSquid(base::StringPiece16 guid) {
  base::string16 squid;
  squid.reserve(32);
  auto* input = guid.begin();
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
