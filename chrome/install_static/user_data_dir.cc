// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/user_data_dir.h"

#include <windows.h>
#include <assert.h>

#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/policy_path_parser.h"

namespace install_static {

namespace {

std::wstring* g_user_data_dir;
std::wstring* g_invalid_user_data_dir;

// Retrieves a registry policy for the user data directory from the registry, if
// one is set. If there's none set in either HKLM or HKCU, |user_data_dir| will
// be unmodified.
void GetUserDataDirFromRegistryPolicyIfSet(const InstallConstants& mode,
                                           std::wstring* user_data_dir) {
  assert(user_data_dir);
  std::wstring policies_path = L"SOFTWARE\\Policies\\";
  AppendChromeInstallSubDirectory(mode, false /* !include_suffix */,
                                  &policies_path);

  std::wstring value;

  constexpr wchar_t kUserDataDirRegistryKeyName[] = L"UserDataDir";

  // First, try HKLM.
  if (nt::QueryRegValueSZ(nt::HKLM, nt::NONE, policies_path.c_str(),
                          kUserDataDirRegistryKeyName, &value)) {
    *user_data_dir = ExpandPathVariables(value);
    return;
  }

  // Second, try HKCU.
  if (nt::QueryRegValueSZ(nt::HKCU, nt::NONE, policies_path.c_str(),
                             kUserDataDirRegistryKeyName, &value)) {
    *user_data_dir = ExpandPathVariables(value);
    return;
  }
}

std::wstring MakeAbsoluteFilePath(const std::wstring& input) {
  wchar_t file_path[MAX_PATH];
  if (!_wfullpath(file_path, input.c_str(), _countof(file_path)))
    return std::wstring();
  return file_path;
}

// The same as GetUserDataDirectory(), but directly queries the global command
// line object for the --user-data-dir flag. This is the more commonly used
// function, where GetUserDataDirectory() is used primiarily for testing.
bool GetUserDataDirectoryUsingProcessCommandLine(
    const InstallConstants& mode,
    std::wstring* result,
    std::wstring* invalid_supplied_directory) {
  return GetUserDataDirectoryImpl(
      GetSwitchValueFromCommandLine(::GetCommandLine(), kUserDataDirSwitch),
      mode, result, invalid_supplied_directory);
}

// Populates |result| with the default User Data directory for the current
// user. Returns false if all attempts at locating a User Data directory fail.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
bool GetDefaultUserDataDirectory(const InstallConstants& mode,
                                 std::wstring* result) {
  // This environment variable should be set on Windows Vista and later
  // (https://msdn.microsoft.com/library/windows/desktop/dd378457.aspx).
  std::wstring user_data_dir = GetEnvironmentString16(L"LOCALAPPDATA");

  if (user_data_dir.empty()) {
    // LOCALAPPDATA was not set; fallback to the temporary files path.
    DWORD size = ::GetTempPath(0, nullptr);
    if (!size)
      return false;
    user_data_dir.resize(size + 1);
    size = ::GetTempPath(size + 1, &user_data_dir[0]);
    if (!size || size >= user_data_dir.size())
      return false;
    user_data_dir.resize(size);
  }

  result->swap(user_data_dir);
  if ((*result)[result->length() - 1] != L'\\')
    result->push_back(L'\\');
  AppendChromeInstallSubDirectory(mode, true /* include_suffix */, result);
  result->push_back(L'\\');
  result->append(L"User Data");
  return true;
}

}  // namespace

bool GetUserDataDirectoryImpl(
    const std::wstring& user_data_dir_from_command_line,
    const InstallConstants& mode,
    std::wstring* result,
    std::wstring* invalid_supplied_directory) {
  std::wstring user_data_dir = user_data_dir_from_command_line;

  GetUserDataDirFromRegistryPolicyIfSet(mode, &user_data_dir);

  // On Windows, trailing separators leave Chrome in a bad state. See
  // crbug.com/464616.
  while (!user_data_dir.empty() &&
         (user_data_dir.back() == '\\' || user_data_dir.back() == '/')) {
    user_data_dir.pop_back();
  }

  bool got_valid_directory =
      !user_data_dir.empty() && RecursiveDirectoryCreate(user_data_dir);
  if (!got_valid_directory) {
    *invalid_supplied_directory = user_data_dir;
    got_valid_directory = GetDefaultUserDataDirectory(mode, &user_data_dir);
  }

  // The Chrome implementation CHECKs() here in the browser process. We
  // don't as this function is used to initialize crash reporting, so
  // we would get no report of this failure.
  assert(got_valid_directory);
  if (!got_valid_directory)
    return false;

  *result = MakeAbsoluteFilePath(user_data_dir);
  return true;
}

bool GetUserDataDirectory(std::wstring* user_data_dir,
                          std::wstring* invalid_user_data_dir) {
  if (!g_user_data_dir) {
    g_user_data_dir = new std::wstring();
    g_invalid_user_data_dir = new std::wstring();
    if (!GetUserDataDirectoryUsingProcessCommandLine(
            InstallDetails::Get().mode(), g_user_data_dir,
            g_invalid_user_data_dir)) {
      return false;
    }
    assert(!g_user_data_dir->empty());
  }
  *user_data_dir = *g_user_data_dir;
  if (invalid_user_data_dir)
    *invalid_user_data_dir = *g_invalid_user_data_dir;
  return true;
}

}  // namespace install_static
