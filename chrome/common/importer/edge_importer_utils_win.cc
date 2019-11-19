// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/edge_importer_utils_win.h"

#include <Shlobj.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/importer/importer_test_registry_overrider_win.h"

namespace {

const base::char16 kEdgeSettingsMainKey[] = L"MicrosoftEdge\\Main";

const base::char16 kEdgePackageName[] =
    L"microsoft.microsoftedge_8wekyb3d8bbwe";

// We assume at the moment that the package name never changes for Edge.
base::string16 GetEdgePackageName() {
  return kEdgePackageName;
}

base::string16 GetEdgeRegistryKey(const base::string16& key_name) {
  base::string16 registry_key =
      L"Software\\Classes\\Local Settings\\"
      L"Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\"
      L"Storage\\";
  registry_key += GetEdgePackageName();
  registry_key += L"\\";
  registry_key += key_name;
  return registry_key;
}

base::string16 GetPotentiallyOverridenEdgeKey(
    const base::string16& desired_key_path) {
  base::string16 test_registry_override(
      ImporterTestRegistryOverrider::GetTestRegistryOverride());
  return test_registry_override.empty() ? GetEdgeRegistryKey(desired_key_path)
                                        : test_registry_override;
}

}  // namespace

namespace importer {

base::string16 GetEdgeSettingsKey() {
  return GetPotentiallyOverridenEdgeKey(kEdgeSettingsMainKey);
}

base::FilePath GetEdgeDataFilePath() {
  wchar_t buffer[MAX_PATH];
  if (::SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                        buffer) != S_OK)
    return base::FilePath();

  base::FilePath base_path(buffer);
  base::string16 rel_path = L"Packages\\";
  rel_path += GetEdgePackageName();
  rel_path += L"\\AC\\MicrosoftEdge\\User\\Default";
  return base_path.Append(rel_path);
}

bool IsEdgeFavoritesLegacyMode() {
  base::win::RegKey key(HKEY_CURRENT_USER, GetEdgeSettingsKey().c_str(),
                        KEY_READ);
  DWORD ese_enabled = 0;
  // Check whether Edge is using the new Extensible Store Engine (ESE) format
  // for its favorites.

  if (key.ReadValueDW(L"FavoritesESEEnabled", &ese_enabled) == ERROR_SUCCESS)
    return !ese_enabled;
  // If the registry key is missing, check the Windows version.
  // Edge switched to ESE in Windows 10 Build 10565 (somewhere between
  // Windows 10 RTM and Windows 10 November 1511 Update).
  return base::win::GetVersion() < base::win::Version::WIN10_TH2;
}

bool EdgeImporterCanImport() {
  base::File::Info file_info;
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return false;
  return base::GetFileInfo(GetEdgeDataFilePath(), &file_info) &&
         file_info.is_directory;
}

}  // namespace importer
