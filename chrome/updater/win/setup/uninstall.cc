// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/uninstall.h"

#include <shlobj.h>
#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/registry_util.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"
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
  }
}

void DeleteComService(bool uninstall_all) {
  DCHECK(::IsUserAnAdmin());

  for (const GUID& appid :
       JoinVectors(GetSideBySideServers(UpdaterScope::kSystem),
                   uninstall_all ? GetActiveServers(UpdaterScope::kSystem)
                                 : std::vector<CLSID>())) {
    installer::DeleteRegistryKey(HKEY_LOCAL_MACHINE,
                                 GetComServerAppidRegistryPath(appid),
                                 WorkItem::kWow64Default);
  }

  for (const bool is_internal_service : {true, false}) {
    if (!uninstall_all && !is_internal_service)
      continue;

    const std::wstring service_name = GetServiceName(is_internal_service);
    if (!installer::InstallServiceWorkItem::DeleteService(
            service_name.c_str(), UPDATER_KEY, {}, {})) {
      LOG(WARNING) << "DeleteService [" << service_name << "] failed.";
    }
  }
}

void DeleteComInterfaces(UpdaterScope scope, bool uninstall_all) {
  for (const IID& iid : JoinVectors(
           GetSideBySideInterfaces(scope),
           uninstall_all ? GetActiveInterfaces(scope) : std::vector<IID>())) {
    for (const auto& reg_path :
         {GetComIidRegistryPath(iid), GetComTypeLibRegistryPath(iid)}) {
      installer::DeleteRegistryKey(UpdaterScopeToHKeyRoot(scope), reg_path,
                                   WorkItem::kWow64Default);
    }
  }
}

void DeleteGoogleUpdateFilesAndKeys(UpdaterScope scope) {
  installer::DeleteRegistryKey(UpdaterScopeToHKeyRoot(scope), UPDATER_KEY,
                               KEY_WOW64_32KEY);

  const absl::optional<base::FilePath> target_path =
      GetGoogleUpdateExePath(scope);
  if (target_path)
    base::DeletePathRecursively(target_path->DirName());
}

int RunUninstallScript(UpdaterScope scope, bool uninstall_all) {
  const absl::optional<base::FilePath> versioned_dir =
      GetVersionedDataDirectory(scope);
  if (!versioned_dir) {
    LOG(ERROR) << "GetVersionedDataDirectory failed.";
    return kErrorNoVersionedDirectory;
  }
  const absl::optional<base::FilePath> base_dir = GetBaseDataDirectory(scope);
  if (IsSystemInstall(scope) && !base_dir) {
    LOG(ERROR) << "GetBaseDataDirectory failed.";
    return kErrorNoBaseDirectory;
  }

  base::FilePath cmd_exe_path;
  if (!base::PathService::Get(base::DIR_SYSTEM, &cmd_exe_path))
    return kErrorPathServiceFailed;
  cmd_exe_path = cmd_exe_path.Append(L"cmd.exe");

  const base::FilePath script_path =
      versioned_dir->AppendASCII(kUninstallScript);

  const std::wstring cmdline = base::StringPrintf(
      L"\"%ls\" /Q /C \"\"%ls\" --dir=\"%ls\"\"", cmd_exe_path.value().c_str(),
      script_path.value().c_str(),
      (uninstall_all ? base_dir : versioned_dir)->value().c_str());
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
  DCHECK(!IsSystemInstall(scope) || ::IsUserAnAdmin());

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);

  updater::UnregisterWakeTask(scope);

  if (uninstall_all)
    DeleteGoogleUpdateFilesAndKeys(scope);

  DeleteComInterfaces(scope, uninstall_all);
  if (IsSystemInstall(scope))
    DeleteComService(uninstall_all);
  DeleteComServer(scope, uninstall_all);

  if (!IsSystemInstall(scope))
    UnregisterUserRunAtStartup(GetTaskNamePrefix(scope));

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
