// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/common/win/install_utils.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/win/current_module.h"
#include "base/win/registry.h"
#include "cloud_print/common/win/cloud_print_utils.h"

namespace cloud_print {

namespace {

// Google Update related constants.
const wchar_t kClientsKey[] = L"SOFTWARE\\Google\\Update\\Clients\\";
const wchar_t kClientStateKey[] = L"SOFTWARE\\Google\\Update\\ClientState\\";
const wchar_t kVersionKey[] = L"pv";
const wchar_t kNameKey[] = L"name";

enum InstallerResult {
  INSTALLER_RESULT_FAILED_CUSTOM_ERROR = 1,
  INSTALLER_RESULT_FAILED_SYSTEM_ERROR = 3,
};

const wchar_t kRegValueInstallerResult[] = L"InstallerResult";
const wchar_t kRegValueInstallerResultUIString[] = L"InstallerResultUIString";
const wchar_t kRegValueInstallerError[] = L"InstallerError";

// Uninstall related constants.
const wchar_t kUninstallKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";
const wchar_t kInstallLocation[] = L"InstallLocation";
const wchar_t kUninstallString[] = L"UninstallString";
const wchar_t kDisplayVersion[] = L"DisplayVersion";
const wchar_t kDisplayIcon[] = L"DisplayIcon";
const wchar_t kDisplayName[] = L"DisplayName";
const wchar_t kPublisher[] = L"Publisher";
const wchar_t kNoModify[] = L"NoModify";
const wchar_t kNoRepair[] = L"NoRepair";

}  // namespace

void SetGoogleUpdateKeys(const std::wstring& product_id,
                         const std::wstring& product_name) {
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE,
                 (cloud_print::kClientsKey + product_id).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
  }

  // Get the version from the resource file.
  std::wstring version_string;
  std::unique_ptr<FileVersionInfo> version_info =
      FileVersionInfo::CreateFileVersionInfoForModule(CURRENT_MODULE());
  if (version_info) {
    version_string = base::AsWString(version_info->product_version());
  } else {
    LOG(ERROR) << "Unable to get version string";
    // Use a random version string so that Google Update has something to go by.
    version_string = L"0.0.0.99";
  }

  if (key.WriteValue(kVersionKey, version_string.c_str()) != ERROR_SUCCESS ||
      key.WriteValue(kNameKey, product_name.c_str()) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to set registry keys";
  }
}

void SetGoogleUpdateError(const std::wstring& product_id,
                          const std::wstring& message) {
  LOG(ERROR) << message;
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE,
                 (cloud_print::kClientStateKey + product_id).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
  }

  if (key.WriteValue(kRegValueInstallerResult,
                     INSTALLER_RESULT_FAILED_CUSTOM_ERROR) != ERROR_SUCCESS ||
      key.WriteValue(kRegValueInstallerResultUIString, message.c_str()) !=
          ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to set registry keys";
  }
}

void SetGoogleUpdateError(const std::wstring& product_id, HRESULT hr) {
  LOG(ERROR) << cloud_print::GetErrorMessage(hr);
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE,
                 (cloud_print::kClientStateKey + product_id).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
  }

  if (key.WriteValue(kRegValueInstallerResult,
                     INSTALLER_RESULT_FAILED_SYSTEM_ERROR) != ERROR_SUCCESS ||
      key.WriteValue(kRegValueInstallerError, hr) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to set registry keys";
  }
}

void DeleteGoogleUpdateKeys(const std::wstring& product_id) {
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE,
               (cloud_print::kClientsKey + product_id).c_str(),
               DELETE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key to delete";
    return;
  }
  if (key.DeleteKey(L"") != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to delete key";
  }
}

void CreateUninstallKey(const std::wstring& uninstall_id,
                        const std::wstring& product_name,
                        const std::string& uninstall_switch) {
  // Now write the Windows Uninstall entries
  // Minimal error checking here since the install can continue
  // if this fails.
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE,
                 (cloud_print::kUninstallKey + uninstall_id).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Unable to open key";
    return;
  }

  base::FilePath unstall_binary;
  CHECK(base::PathService::Get(base::FILE_EXE, &unstall_binary));

  base::CommandLine uninstall_command(unstall_binary);
  uninstall_command.AppendSwitch(uninstall_switch);
  key.WriteValue(kUninstallString,
                 uninstall_command.GetCommandLineString().c_str());
  key.WriteValue(kInstallLocation, unstall_binary.DirName().value().c_str());

  // Get the version resource.
  std::unique_ptr<FileVersionInfo> version_info =
      FileVersionInfo::CreateFileVersionInfoForModule(CURRENT_MODULE());

  if (version_info) {
    key.WriteValue(kDisplayVersion,
                   base::as_wcstr(version_info->file_version()));
    key.WriteValue(kPublisher, base::as_wcstr(version_info->company_name()));
  } else {
    LOG(ERROR) << "Unable to get version string";
  }
  key.WriteValue(kDisplayName, product_name.c_str());
  key.WriteValue(kDisplayIcon, unstall_binary.value().c_str());
  key.WriteValue(kNoModify, 1);
  key.WriteValue(kNoRepair, 1);
}

void DeleteUninstallKey(const std::wstring& uninstall_id) {
  ::RegDeleteKey(HKEY_LOCAL_MACHINE,
                 (cloud_print::kUninstallKey + uninstall_id).c_str());
}

base::FilePath GetInstallLocation(const std::wstring& uninstall_id) {
  base::win::RegKey key;
  if (key.Open(HKEY_LOCAL_MACHINE,
               (cloud_print::kUninstallKey + uninstall_id).c_str(),
               KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    // Not installed.
    return base::FilePath();
  }
  std::wstring install_path_value;
  key.ReadValue(kInstallLocation, &install_path_value);
  return base::FilePath(install_path_value);
}

void DeleteProgramDir(const std::string& delete_switch) {
  base::FilePath installer_source;
  if (!base::PathService::Get(base::FILE_EXE, &installer_source))
    return;
  // Deletes only subdirs of program files.
  if (!IsProgramsFilesParent(installer_source))
    return;
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path))
    return;
  base::CopyFile(installer_source, temp_path);
  base::DeleteFileAfterReboot(temp_path);
  base::CommandLine command_line(temp_path);
  command_line.AppendSwitchPath(delete_switch, installer_source.DirName());
  base::LaunchOptions options;
  if (!base::LaunchProcess(command_line, options).IsValid()) {
    LOG(ERROR) << "Unable to launch child uninstall.";
  }
}

bool IsProgramsFilesParent(const base::FilePath& path) {
  base::FilePath program_files;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files))
    return false;
  return program_files.IsParent(path);
}

}  // namespace cloud_print
