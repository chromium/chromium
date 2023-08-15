// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/uninstallation_via_os_settings.h"

#include <windows.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"

namespace {

// Win32 App registry entry for uninstallation. System detects the registry
// and show its App for App or Remove Settings.
constexpr wchar_t kUninstallRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

// List of registry specific LONG error codes obtained from
// winerror.h in depot_tools/ toolchain as well as
// base/win/windows_types.h.
enum class WinRegistryErrorCode {
  kErrorOther = 0,
  kErrorSuccess = 1,
  kErrorFileNotFound = 2,
  kErrorAccessDenied = 3,
  kErrorInvalidHandle = 4,
  kErrorSharingViolation = 5,
  kErrorLockViolation = 6,
  kErrorMoreData = 7,
  kErrorRegistryRecovered = 8,
  kErrorRegistryCorrupt = 9,
  kErrorRegistryIOFailed = 10,
  kErrorNotRegistryFile = 11,
  kErrorRegistryQuotaLimit = 12,
  kErrorRegistryHiveRecovered = 13,
  kErrorRegistryClusterInvalidFunction = 14,
  kMaxValue = kErrorRegistryClusterInvalidFunction
};

void RecordUninstallationRegistrationOSResult(LONG result) {
  WinRegistryErrorCode final_code = WinRegistryErrorCode::kErrorOther;
  switch (result) {
    case ERROR_SUCCESS:
      final_code = WinRegistryErrorCode::kErrorSuccess;
      break;
    case ERROR_FILE_NOT_FOUND:
      final_code = WinRegistryErrorCode::kErrorFileNotFound;
      break;
    case ERROR_ACCESS_DENIED:
      final_code = WinRegistryErrorCode::kErrorAccessDenied;
      break;
    case ERROR_INVALID_HANDLE:
      final_code = WinRegistryErrorCode::kErrorInvalidHandle;
      break;
    case ERROR_SHARING_VIOLATION:
      final_code = WinRegistryErrorCode::kErrorSharingViolation;
      break;
    case ERROR_LOCK_VIOLATION:
      final_code = WinRegistryErrorCode::kErrorLockViolation;
      break;
    case ERROR_MORE_DATA:
      final_code = WinRegistryErrorCode::kErrorMoreData;
      break;
    case ERROR_REGISTRY_RECOVERED:
      final_code = WinRegistryErrorCode::kErrorRegistryRecovered;
      break;
    case ERROR_REGISTRY_CORRUPT:
      final_code = WinRegistryErrorCode::kErrorRegistryCorrupt;
      break;
    case ERROR_REGISTRY_IO_FAILED:
      final_code = WinRegistryErrorCode::kErrorRegistryIOFailed;
      break;
    case ERROR_NOT_REGISTRY_FILE:
      final_code = WinRegistryErrorCode::kErrorNotRegistryFile;
      break;
    case ERROR_REGISTRY_QUOTA_LIMIT:
      final_code = WinRegistryErrorCode::kErrorRegistryQuotaLimit;
      break;
    case ERROR_REGISTRY_HIVE_RECOVERED:
      final_code = WinRegistryErrorCode::kErrorRegistryHiveRecovered;
      break;
    case ERROR_CLUSTER_REGISTRY_INVALID_FUNCTION:
      final_code = WinRegistryErrorCode::kErrorRegistryClusterInvalidFunction;
      break;
    default:
      break;
  }
  base::UmaHistogramEnumeration(
      "WebApp.OsSettingsUninstallUnregistration.WinOSResult", final_code);
}

}  // namespace

bool RegisterUninstallationViaOsSettings(
    const std::wstring& key,
    const std::wstring& display_name,
    const std::wstring& publisher,
    const base::CommandLine& uninstall_command,
    const base::FilePath& icon_path) {
  DCHECK(!key.empty());
  DCHECK(!display_name.empty());
  DCHECK(!publisher.empty());
  DCHECK(!uninstall_command.GetProgram().empty());

  base::win::RegKey uninstall_reg_key;
  LONG result = uninstall_reg_key.Open(HKEY_CURRENT_USER, kUninstallRegistryKey,
                                       KEY_CREATE_SUB_KEY);
  if (result != ERROR_SUCCESS)
    return false;

  base::win::RegKey uninstall_reg_entry_key;
  DWORD disposition;

  result = uninstall_reg_entry_key.CreateWithDisposition(
      uninstall_reg_key.Handle(), key.c_str(), &disposition, KEY_WRITE);
  if (result != ERROR_SUCCESS)
    return false;

  if (disposition != REG_CREATED_NEW_KEY)
    return false;

  // Add Uninstall values. Windows will show the icon at index 0
  // if no index is specified in this value.
  uninstall_reg_entry_key.WriteValue(L"DisplayIcon", icon_path.value().c_str());
  uninstall_reg_entry_key.WriteValue(L"DisplayName", display_name.c_str());
  uninstall_reg_entry_key.WriteValue(L"DisplayVersion", L"1.0");
  uninstall_reg_entry_key.WriteValue(L"ApplicationVersion", L"1.0");

  static constexpr wchar_t kDateFormat[] = L"yyyyyMMdd";
  wchar_t date_str[std::size(kDateFormat)] = {};
  int len = ::GetDateFormatW(LOCALE_INVARIANT, 0, nullptr, kDateFormat,
                             date_str, std::size(date_str));
  if (len)
    uninstall_reg_entry_key.WriteValue(L"InstallDate", date_str);

  uninstall_reg_entry_key.WriteValue(L"Publisher", publisher.c_str());
  uninstall_reg_entry_key.WriteValue(
      L"UninstallString", uninstall_command.GetCommandLineString().c_str());
  uninstall_reg_entry_key.WriteValue(L"NoRepair", 1);
  uninstall_reg_entry_key.WriteValue(L"NoModify", 1);

  return true;
}

bool UnregisterUninstallationViaOsSettings(const std::wstring& name) {
  base::win::RegKey uninstall_reg_key;
  LONG result = uninstall_reg_key.Open(HKEY_CURRENT_USER, kUninstallRegistryKey,
                                       KEY_QUERY_VALUE);
  if (result == ERROR_FILE_NOT_FOUND) {
    RecordUninstallationRegistrationOSResult(result);
    return true;
  } else if (result != ERROR_SUCCESS) {
    RecordUninstallationRegistrationOSResult(result);
    return false;
  }

  LONG delete_key_result = uninstall_reg_key.DeleteKey(name.c_str());
  RecordUninstallationRegistrationOSResult(delete_key_result);
  // DeleteKey and Open work with different security access attributes, so
  // ERROR_FILE_NOT_FOUND also needs to be treated as success during deletion.
  return delete_key_result == ERROR_SUCCESS ||
         delete_key_result == ERROR_FILE_NOT_FOUND;
}
