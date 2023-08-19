// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include <array>
#include <string>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"

namespace {

// Returns the path denoted by `key`. If `base::PathService` fails to return the
// path to a directory that exists, the value of the environment variable
// corresponding to `key`, if any, is used if it names an absolute directory
// that exists. Returns an empty path if all attempts fail.
base::FilePath GetPathWithEnvironmentFallback(int key) {
  if (base::FilePath path; base::PathService::Get(key, &path) &&
                           !path.empty() && base::DirectoryExists(path)) {
    return path;
  }

  static constexpr auto kKeyToVariable =
      base::MakeFixedFlatMapSorted<int, base::WStringPiece>(
          {{base::DIR_PROGRAM_FILES, L"PROGRAMFILES"},
           {base::DIR_PROGRAM_FILESX86, L"PROGRAMFILES(X86)"},
           {base::DIR_LOCAL_APP_DATA, L"LOCALAPPDATA"}});
  if (auto* it = kKeyToVariable.find(key); it != kKeyToVariable.end()) {
    std::array<wchar_t, MAX_PATH> value;
    value[0] = L'\0';
    if (DWORD ret = ::GetEnvironmentVariableW(it->second.data(), value.data(),
                                              value.size());
        ret && ret < value.size()) {
      if (base::FilePath path(value.data()); path.IsAbsolute() &&
                                             !path.ReferencesParent() &&
                                             base::DirectoryExists(path)) {
        return path;
      }
    }
  }

  return {};
}

// Returns a valid file path with the proper casing from the system if `prefs`
// has a `distribution.program_files_dir` value that is a valid target for the
// browser's installation and `system_install` is true. Returns an empty file
// path otherwise.
base::FilePath GetInstallationDirFromPrefs(
    const installer::InitialPreferences& prefs,
    bool system_install) {
  base::FilePath program_files_dir;
  if (system_install) {
    prefs.GetPath(installer::initial_preferences::kProgramFilesDir,
                  &program_files_dir);
  }
  if (program_files_dir.empty())
    return program_files_dir;

  base::FilePath expected_dir;
  bool valid_program_files_path =
      ((!(expected_dir =
              GetPathWithEnvironmentFallback(base::DIR_PROGRAM_FILES))
             .empty() &&
        base::FilePath::CompareEqualIgnoreCase(program_files_dir.value(),
                                               expected_dir.value())) ||
       (!(expected_dir =
              GetPathWithEnvironmentFallback(base::DIR_PROGRAM_FILESX86))
             .empty() &&
        base::FilePath::CompareEqualIgnoreCase(program_files_dir.value(),
                                               expected_dir.value())));

  return valid_program_files_path
             ? expected_dir
                   .Append(install_static::GetChromeInstallSubDirectory())
                   .Append(installer::kInstallBinaryDir)
             : base::FilePath();
}

// Returns the default install path given an install level.
base::FilePath GetDefaultChromeInstallPathChecked(bool system_install) {
  base::FilePath install_path = GetPathWithEnvironmentFallback(
      system_install ? base::DIR_PROGRAM_FILES : base::DIR_LOCAL_APP_DATA);

  // Later steps assume a valid install path was found.
  CHECK(!install_path.empty());
  return install_path.Append(install_static::GetChromeInstallSubDirectory())
      .Append(installer::kInstallBinaryDir);
}

// Returns the path to the installation at `system_install` provided that the
// browser is installed and its `UninstallString` points into a valid install
// directory.
base::FilePath GetCurrentInstallPathFromRegistry(bool system_install) {
  base::FilePath install_path;

  if (!InstallUtil::GetChromeVersion(system_install).IsValid()) {
    return install_path;
  }

  std::wstring uninstall_string;
  const HKEY root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey(root, install_static::GetClientStateKeyPath().c_str(),
                    KEY_QUERY_VALUE | KEY_WOW64_32KEY)
      .ReadValue(installer::kUninstallStringField, &uninstall_string);
  if (uninstall_string.empty()) {
    return install_path;
  }

  base::FilePath setup_path(std::move(uninstall_string));
  if (!setup_path.IsAbsolute() || setup_path.ReferencesParent()) {
    return install_path;
  }

  // The UninstallString has the format
  // [InstallPath]/[version]/Installer/setup.exe. In order to get the
  // [InstallPath], the full path must be pruned of the last 3 components.
  install_path = setup_path.DirName().DirName().DirName();

  // The install path must not be at the root of the volume and must exist.
  if (install_path == install_path.DirName() ||
      !base::DirectoryExists(install_path)) {
    install_path = base::FilePath();
  }

  return install_path;
}

}  // namespace

namespace installer {

base::FilePath GetInstalledDirectory(bool system_install) {
  return GetCurrentInstallPathFromRegistry(system_install);
}

base::FilePath GetDefaultChromeInstallPath(bool system_install) {
  return GetDefaultChromeInstallPathChecked(system_install);
}

base::FilePath GetChromeInstallPathWithPrefs(bool system_install,
                                             const InitialPreferences& prefs) {
  base::FilePath install_path =
      GetCurrentInstallPathFromRegistry(system_install);
  if (!install_path.empty())
    return install_path;

  install_path = GetInstallationDirFromPrefs(prefs, system_install);
  if (install_path.empty())
    install_path = GetDefaultChromeInstallPathChecked(system_install);
  return install_path;
}

}  // namespace installer.
