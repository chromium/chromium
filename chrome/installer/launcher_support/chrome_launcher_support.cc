// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/launcher_support/chrome_launcher_support.h"

#include <windows.h>

#include <string>

#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"

namespace chrome_launcher_support {

namespace {

// TODO(huangs) Refactor the constants: http://crbug.com/148538
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kUpdateClientStateRegKey[] =
    L"Software\\Google\\Update\\ClientState";

const wchar_t kUpdateClientsRegKey[] = L"Software\\Google\\Update\\Clients";

// Copied from google_chrome_install_modes.cc.
const wchar_t kBrowserAppGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kSxSBrowserAppGuid[] = L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";
#else
const wchar_t kInstallationRegKey[] = L"Software\\Chromium";
#endif

// Copied from util_constants.cc.
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kUninstallStringField[] = L"UninstallString";
const wchar_t kVersionStringField[] = L"pv";

// Returns the registry path to where Client state is stored.
std::wstring GetClientStateRegKey() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return kUpdateClientStateRegKey;
#else
  return kInstallationRegKey;
#endif
}

// Returns the registry path to where basic information about the Clients
// like name and version information are stored.
std::wstring GetClientsRegKey() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return kUpdateClientsRegKey;
#else
  return kInstallationRegKey;
#endif
}

// Reads a string value from the specified product's registry key. Returns true
// iff the value is present and successfully read.
bool GetValueFromRegistry(InstallationLevel level,
                          const std::wstring key_path,
                          const wchar_t* app_guid,
                          const wchar_t* value_name,
                          std::wstring* value) {
  HKEY root_key = (level == USER_LEVEL_INSTALLATION) ? HKEY_CURRENT_USER
                                                     : HKEY_LOCAL_MACHINE;
  std::wstring subkey(key_path);
  if (app_guid)
    subkey.append(1, L'\\').append(app_guid);
  base::win::RegKey reg_key;
  // Google Update always uses 32bit hive.
  if (reg_key.Open(root_key, subkey.c_str(),
                   KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    if (reg_key.ReadValue(value_name, value) == ERROR_SUCCESS) {
      return true;
    }
  }
  return false;
}

// Reads the path to setup.exe from the value "UninstallString" within the
// specified product's registry key. Returns an empty FilePath if an error
// occurs or the product is not installed at the specified level.
base::FilePath GetSetupExeFromRegistry(InstallationLevel level,
                                       const wchar_t* app_guid) {
  std::wstring uninstall;
  if (GetValueFromRegistry(level, GetClientStateRegKey(), app_guid,
                           kUninstallStringField, &uninstall)) {
    base::FilePath setup_exe_path(uninstall);
    if (base::PathExists(setup_exe_path))
      return setup_exe_path;
  }
  return base::FilePath();
}

// Returns the path to an existing setup.exe at the specified level, if it can
// be found via the registry.
base::FilePath GetSetupExeForInstallationLevel(InstallationLevel level) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Look in the registry for Chrome.
  return GetSetupExeFromRegistry(level, kBrowserAppGuid);
#else
  // For Chromium, there are no GUIDs. Just look in the Chromium registry key.
  return GetSetupExeFromRegistry(level, nullptr);
#endif
}

// Returns the path to an installed |exe_file| (e.g. chrome.exe) at the
// specified level, given |setup_exe_path| from the registry.  Returns empty
// base::FilePath if none found, or if |setup_exe_path| is empty.
base::FilePath FindExeRelativeToSetupExe(const base::FilePath setup_exe_path,
                                         const wchar_t* exe_file) {
  if (!setup_exe_path.empty()) {
    // The uninstall path contains the path to setup.exe, which is two levels
    // down from |exe_file|. Move up two levels (plus one to drop the file
    // name) and look for chrome.exe from there.
    base::FilePath exe_path(
        setup_exe_path.DirName().DirName().DirName().Append(exe_file));
    if (base::PathExists(exe_path))
      return exe_path;
    // By way of mild future proofing, look up one to see if there's a
    // |exe_file| in the version directory
    exe_path = setup_exe_path.DirName().DirName().Append(exe_file);
    if (base::PathExists(exe_path))
      return exe_path;
  }
  return base::FilePath();
}

}  // namespace

base::FilePath GetChromePathForInstallationLevel(InstallationLevel level,
                                                 bool is_sxs) {
  if (is_sxs) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    return FindExeRelativeToSetupExe(
        GetSetupExeFromRegistry(level, kSxSBrowserAppGuid), kChromeExe);
#else
    // There is no SxS build for Chromium.
    return base::FilePath();
#endif
  } else {
    return FindExeRelativeToSetupExe(GetSetupExeForInstallationLevel(level),
                                     kChromeExe);
  }
}

base::FilePath GetAnyChromePath(bool is_sxs) {
  base::FilePath path;
  path = GetChromePathForInstallationLevel(SYSTEM_LEVEL_INSTALLATION, is_sxs);
  if (path.empty())
    path = GetChromePathForInstallationLevel(USER_LEVEL_INSTALLATION, is_sxs);
  return path;
}

base::Version GetChromeVersionForInstallationLevel(InstallationLevel level,
                                                   bool is_sxs) {
  const wchar_t* app_guid = nullptr;  // Chromium doesn't use App GUIDs.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  app_guid = is_sxs ? kSxSBrowserAppGuid : kBrowserAppGuid;
#else
  // There is no SxS build for Chromium.
  if (is_sxs)
    return base::Version();
#endif

  std::wstring version_str;
  if (GetValueFromRegistry(level, GetClientsRegKey(), app_guid,
                           kVersionStringField, &version_str)) {
    base::Version version(base::WideToASCII(version_str));
    if (version.IsValid())
      return version;
  }
  return base::Version();
}

}  // namespace chrome_launcher_support
