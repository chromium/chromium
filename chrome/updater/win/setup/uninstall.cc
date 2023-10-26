// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/uninstall.h"

#include <shlobj.h>
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/registry_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/win_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

void DeleteComServer(UpdaterScope scope, bool uninstall_all) {
  for (const CLSID& clsid : JoinVectors(
           GetSideBySideServers(scope),
           uninstall_all ? GetActiveServers(scope) : std::vector<CLSID>())) {
    installer::DeleteRegistryKey(UpdaterScopeToHKeyRoot(scope),
                                 GetComServerClsidRegistryPath(clsid),
                                 WorkItem::kWow64Default);

    const std::wstring progid(GetProgIdForClsid(clsid));
    if (!progid.empty()) {
      installer::DeleteRegistryKey(UpdaterScopeToHKeyRoot(scope),
                                   GetComProgIdRegistryPath(progid),
                                   WorkItem::kWow64Default);
    }
  }
}

void DeleteComService(bool uninstall_all) {
  CHECK(::IsUserAnAdmin());

  for (const GUID& appid :
       JoinVectors(GetSideBySideServers(UpdaterScope::kSystem),
                   uninstall_all ? GetActiveServers(UpdaterScope::kSystem)
                                 : std::vector<CLSID>())) {
    installer::DeleteRegistryKey(HKEY_LOCAL_MACHINE,
                                 GetComServerAppidRegistryPath(appid),
                                 WorkItem::kWow64Default);
  }

  for (const bool is_internal_service : {true, false}) {
    const std::wstring service_name = GetServiceName(is_internal_service);
    if (!installer::InstallServiceWorkItem::DeleteService(
            service_name.c_str(), UPDATER_KEY, {}, {})) {
      LOG(WARNING) << "DeleteService [" << service_name << "] failed.";
    } else {
      VLOG(1) << "DeleteService [" << service_name << "] succeeded.";
    }
  }
}

void DeleteComInterfaces(UpdaterScope scope, bool uninstall_all) {
  for (const auto& [iid, interface_name] : JoinVectors(
           GetSideBySideInterfaces(scope),
           uninstall_all ? GetActiveInterfaces(scope)
                         : std::vector<std::pair<IID, std::wstring>>())) {
    {
      const std::wstring reg_path = GetComIidRegistryPath(iid);
      for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY}) {
        installer::DeleteRegistryKey(UpdaterScopeToHKeyRoot(scope), reg_path,
                                     key_flag);
      }
    }
    {
      const std::wstring reg_path = GetComTypeLibRegistryPath(iid);
      installer::DeleteRegistryKey(UpdaterScopeToHKeyRoot(scope), reg_path,
                                   WorkItem::kWow64Default);
    }
  }
}

void DeleteUpdaterKey(UpdaterScope scope) {
  const HKEY root = UpdaterScopeToHKeyRoot(scope);

  // Delete all the sub keys of `UPDATER_KEY`.
  std::vector<std::wstring> sub_keys;
  for (base::win::RegistryKeyIterator updater_key_iter(root, UPDATER_KEY,
                                                       KEY_WOW64_32KEY);
       updater_key_iter.Valid(); ++updater_key_iter) {
    sub_keys.push_back(updater_key_iter.Name());
  }

  for (const auto& sub_key : sub_keys) {
    const std::wstring subkey_path =
        base::StrCat({UPDATER_KEY, L"\\", sub_key});
    installer::DeleteRegistryKey(root, subkey_path, KEY_WOW64_32KEY);
  }

  // Delete all the values of `UPDATER_KEY`, except the `LastInstaller*` values.
  std::vector<std::wstring> values;
  for (base::win::RegistryValueIterator updater_value_iter(root, UPDATER_KEY,
                                                           KEY_WOW64_32KEY);
       updater_value_iter.Valid(); ++updater_value_iter) {
    if (!base::Contains(kRegValuesLastInstaller, updater_value_iter.Name())) {
      values.push_back(updater_value_iter.Name());
    }
  }
  for (const auto& value : values) {
    installer::DeleteRegistryValue(root, UPDATER_KEY, KEY_WOW64_32KEY, value);
  }

  // Finally, delete `UPDATER_KEY` if it is empty.
  base::win::RegKey updater_key;
  if (updater_key.Open(root, UPDATER_KEY, Wow6432(KEY_QUERY_VALUE)) ==
          ERROR_SUCCESS &&
      updater_key.GetValueCount().value_or(1) == 0) {
    updater_key.DeleteKey(L"", base::win::RegKey::RecursiveDelete(false));
  }
}

void DeleteGoogleUpdateFilesAndKeys(UpdaterScope scope) {
  DeleteUpdaterKey(scope);

  const absl::optional<base::FilePath> target_path =
      GetGoogleUpdateExePath(scope);
  if (target_path) {
    base::DeletePathRecursively(target_path->DirName());
  }
}

int RunUninstallScript(UpdaterScope scope, bool uninstall_all) {
  const absl::optional<base::FilePath> versioned_dir =
      GetVersionedInstallDirectory(scope);
  if (!versioned_dir) {
    LOG(ERROR) << "GetVersionedInstallDirectory failed.";
    return kErrorNoVersionedDirectory;
  }
  const absl::optional<base::FilePath> base_dir = GetInstallDirectory(scope);
  if (IsSystemInstall(scope) && !base_dir) {
    LOG(ERROR) << "GetInstallDirectory failed.";
    return kErrorNoBaseDirectory;
  }

  base::FilePath cmd_exe_path;
  if (!base::PathService::Get(base::DIR_SYSTEM, &cmd_exe_path)) {
    return kErrorPathServiceFailed;
  }
  cmd_exe_path = cmd_exe_path.Append(L"cmd.exe");

  const base::FilePath script_path =
      versioned_dir->AppendASCII(kUninstallScript);

  const std::wstring cmdline = base::StrCat(
      {L"\"", cmd_exe_path.value(), L"\" /Q /C \"\"", script_path.value(),
       L"\" --dir=\"", (uninstall_all ? base_dir : versioned_dir)->value(),
       L"\"\""});
  base::LaunchOptions options;
  options.start_hidden = true;

  VLOG(1) << "Running " << cmdline;

  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to create process " << cmdline;
    return kErrorProcessLaunchFailed;
  }
  return kErrorOk;
}

// Reverses the changes made by setup. This is a best effort uninstall:
// 1. Deletes the scheduled task.
// 2. Deletes the Clients and ClientState keys.
// 3. Runs the uninstall script in the install directory of the updater.
// The execution of this function and the script race each other but the script
// loops and waits in between iterations trying to delete the install directory.
// If `uninstall_all` is set to `true`, the function uninstalls both the
// internal as well as the active updater. If `uninstall_all` is set to `false`,
// the function uninstalls only the internal updater.
int UninstallImpl(UpdaterScope scope, bool uninstall_all) {
  VLOG(1) << __func__ << ", scope: " << scope;
  CHECK(!IsSystemInstall(scope) || ::IsUserAnAdmin());

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);

  updater::UnregisterWakeTask(scope);

  if (uninstall_all) {
    DeleteGoogleUpdateFilesAndKeys(scope);
  }

  DeleteComInterfaces(scope, uninstall_all);
  if (IsSystemInstall(scope)) {
    DeleteComService(uninstall_all);
  }
  DeleteComServer(scope, uninstall_all);

  if (!IsSystemInstall(scope)) {
    UnregisterUserRunAtStartup(GetTaskNamePrefix(scope));
  }

  return RunUninstallScript(scope, uninstall_all);
}

}  // namespace

int Uninstall(UpdaterScope scope) {
  return UninstallImpl(scope, true);
}

// Uninstalls this version of the updater, without uninstalling any other
// versions. This version is assumed to not be the active version.
int UninstallCandidate(UpdaterScope scope) {
  return UninstallImpl(scope, false);
}

}  // namespace updater
