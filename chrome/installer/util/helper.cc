// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/helper.h"

#include "base/path_service.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"

namespace {

base::FilePath GetDefaultChromeInstallPath(bool system_install) {
  base::FilePath install_path;
  int key = system_install ? base::DIR_PROGRAM_FILES : base::DIR_LOCAL_APP_DATA;
  if (base::PathService::Get(key, &install_path)) {
    install_path =
        install_path.Append(install_static::GetChromeInstallSubDirectory());
    install_path = install_path.Append(installer::kInstallBinaryDir);
  }
  return install_path;
}

base::FilePath GetCurrentInstallPathFromRegistry(bool system_install) {
  base::FilePath install_path;
  if (!InstallUtil::GetChromeVersion(system_install).IsValid())
    return install_path;

  std::wstring uninstall_string;
  base::win::RegKey key(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                        install_static::GetClientStateKeyPath().c_str(),
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  key.ReadValue(installer::kUninstallStringField, &uninstall_string);

  // The UninstallString has the format
  // [InstallPath]/[version]/Installer/setup.exe. In order to get the
  // [InstallPath], the full path must be pruned of the last 3 components.
  if (!uninstall_string.empty()) {
    install_path = base::FilePath(std::move(uninstall_string))
                       .DirName()
                       .DirName()
                       .DirName();
  }
  return install_path;
}

}  // namespace

namespace installer {

base::FilePath GetChromeInstallPath(bool system_install) {
  base::FilePath install_path =
      GetCurrentInstallPathFromRegistry(system_install);
  if (install_path.empty())
    install_path = GetDefaultChromeInstallPath(system_install);
  return install_path;
}

}  // namespace installer.
