// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/uninstallation_via_os_settings.h"

#include <windows.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"

namespace {

// Win32 App registry entry for uninstallation. System detects the registry
// and show its App for App or Remove Settings.
constexpr wchar_t kUninstallRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

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

void UnregisterUninstallationViaOsSettings(const std::wstring& name) {
  base::win::RegKey uninstall_reg_key;
  LONG result = uninstall_reg_key.Open(HKEY_CURRENT_USER, kUninstallRegistryKey,
                                       KEY_QUERY_VALUE);
  if (result != ERROR_SUCCESS)
    return;

  uninstall_reg_key.DeleteKey(name.c_str());
}
