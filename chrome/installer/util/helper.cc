// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include <array>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/installation_state.h"
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
      base::MakeFixedFlatMap<int, std::wstring_view>(
          {{base::DIR_PROGRAM_FILES, L"PROGRAMFILES"},
           {base::DIR_PROGRAM_FILESX86, L"PROGRAMFILES(X86)"},
           {base::DIR_PROGRAM_FILES6432, L"ProgramW6432"},
           {base::DIR_LOCAL_APP_DATA, L"LOCALAPPDATA"}});
  if (auto it = kKeyToVariable.find(key); it != kKeyToVariable.end()) {
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
  installer::ProductState product_state;
  if (!product_state.Initialize(system_install)) {
    return {};
  }

  const base::FilePath setup_path =
      product_state.uninstall_command().GetProgram();
  if (setup_path.empty() || !setup_path.IsAbsolute() ||
      setup_path.ReferencesParent()) {
    return {};
  }

  // The path to setup.exe has the format
  // [InstallPath]/[version]/Installer/setup.exe. In order to get the
  // [InstallPath], the full path must be pruned of the last 3 components.
  const base::FilePath install_path = setup_path.DirName().DirName().DirName();

  // The install path must not be at the root of the volume and must exist.
  if (install_path == install_path.DirName() ||
      !base::DirectoryExists(install_path)) {
    return {};
  }

  return install_path;
}

// Returns path keys for the standard installation locations for either
// per-user or per-machine installs. In cases where more than one location is
// possible, the default is always first.
base::span<const int> GetInstallationPathKeys(bool system_install) {
  if (!system_install) {
    // %LOCALAPPDATA% is the only location for per-user installs.
    static constexpr int kPerUserKeys[] = {base::DIR_LOCAL_APP_DATA};
    return base::make_span(kPerUserKeys);
  }
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    // %PROGRAMFILES% is the only location for 32-bit Windows.
    static constexpr int kPerMachineKeys[] = {base::DIR_PROGRAM_FILES};
    return base::make_span(kPerMachineKeys);
  }
  // %PROGRAMFILES%, which matches the current binary's bitness, is the default
  // for 64-bit Windows (x64 and arm64). The "opposite" location is the
  // secondary.
  static constexpr int kx64PerMachineKeys[] = {
      base::DIR_PROGRAM_FILES,  // Native folder for this bitness.
#if defined(ARCH_CPU_64_BITS)
      base::DIR_PROGRAM_FILESX86,  // Folder for 32-bit apps.
#else
      base::DIR_PROGRAM_FILES6432,  // Folder for 64-bit apps.
#endif
  };
  return base::make_span(kx64PerMachineKeys);
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

base::FilePath FindInstallPath(bool system_install,
                               const base::Version& version) {
  CHECK(version.IsValid());

  // Is there an installation in one of the standard locations with a matching
  // version directory?
  for (int path_key : GetInstallationPathKeys(system_install)) {
    if (auto path = GetPathWithEnvironmentFallback(path_key); !path.empty()) {
      path = path.Append(install_static::GetChromeInstallSubDirectory())
                 .Append(kInstallBinaryDir)
                 .AppendASCII(version.GetString());
      if (base::DirectoryExists(path)) {
        return path;
      }
    }
  }
  return {};
}

}  // namespace installer.
