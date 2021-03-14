// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/uninstall.h"

#include <shlobj.h>
#include <windows.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"

namespace updater {
namespace {

void DeleteComServer(HKEY root) {
  for (const auto& clsid :
       {__uuidof(UpdaterClass), __uuidof(UpdaterInternalClass),
        __uuidof(GoogleUpdate3WebUserClass)}) {
    InstallUtil::DeleteRegistryKey(root, GetComServerClsidRegistryPath(clsid),
                                   WorkItem::kWow64Default);
  }
}

void DeleteComService() {
  DCHECK(::IsUserAnAdmin());

  InstallUtil::DeleteRegistryKey(HKEY_LOCAL_MACHINE,
                                 GetComServiceClsidRegistryPath(),
                                 WorkItem::kWow64Default);
  InstallUtil::DeleteRegistryKey(HKEY_LOCAL_MACHINE,
                                 GetComServiceAppidRegistryPath(),
                                 WorkItem::kWow64Default);
  if (!installer::InstallServiceWorkItem::DeleteService(
          kWindowsServiceName, base::ASCIIToWide(UPDATER_KEY),
          {__uuidof(UpdaterServiceClass)}, {}))
    LOG(WARNING) << "DeleteService failed.";
}

void DeleteComInterfaces(HKEY root) {
  for (const auto& iid : GetActiveInterfaces()) {
    for (const auto& reg_path :
         {GetComIidRegistryPath(iid), GetComTypeLibRegistryPath(iid)}) {
      InstallUtil::DeleteRegistryKey(root, reg_path, WorkItem::kWow64Default);
    }
  }
  // TODO(crbug.com/1175095): Support candidate-specific uninstallation.
  for (const auto& iid : GetSideBySideInterfaces()) {
    for (const auto& reg_path :
         {GetComIidRegistryPath(iid), GetComTypeLibRegistryPath(iid)}) {
      InstallUtil::DeleteRegistryKey(root, reg_path, WorkItem::kWow64Default);
    }
  }
}

int RunUninstallScript(bool uninstall_all) {
  base::Optional<base::FilePath> versioned_dir = GetVersionedDirectory();
  if (!versioned_dir) {
    LOG(ERROR) << "GetVersionedDirectory failed.";
    return -1;
  }

  wchar_t cmd_path[MAX_PATH] = {0};
  DWORD size = ExpandEnvironmentStrings(L"%SystemRoot%\\System32\\cmd.exe",
                                        cmd_path, base::size(cmd_path));
  if (!size || size >= MAX_PATH)
    return -1;

  base::FilePath script_path = versioned_dir->AppendASCII(kUninstallScript);

  std::wstring cmdline = cmd_path;
  base::StringAppendF(&cmdline, L" /Q /C \"%ls\" %ls",
                      script_path.value().c_str(),
                      uninstall_all ? L"all" : L"local");
  base::LaunchOptions options;
  options.start_hidden = true;

  VLOG(1) << "Running " << cmdline;

  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to create process " << cmdline;
    return -1;
  }
  return 0;
}

}  // namespace

// Reverses the changes made by setup. This is a best effort uninstall:
// 1. Deletes the scheduled task.
// 2. Deletes the Clients and ClientState keys.
// 3. Runs the uninstall script in the install directory of the updater.
// The execution of this function and the script race each other but the script
// loops and waits in between iterations trying to delete the install directory.
int Uninstall(UpdaterScope scope) {
  VLOG(1) << __func__ << ", scope: " << scope;
  DCHECK(scope == UpdaterScope::kUser || ::IsUserAnAdmin());
  HKEY key =
      scope == UpdaterScope::kSystem ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);

  updater::UnregisterWakeTask();

  std::unique_ptr<WorkItemList> uninstall_list(WorkItem::CreateWorkItemList());
  uninstall_list->AddDeleteRegKeyWorkItem(key, base::ASCIIToWide(UPDATER_KEY),
                                          WorkItem::kWow64Default);
  if (!uninstall_list->Do()) {
    LOG(ERROR) << "Failed to delete the registry keys.";
    uninstall_list->Rollback();
    return -1;
  }

  DeleteComInterfaces(key);
  if (scope == UpdaterScope::kSystem)
    DeleteComService();
  DeleteComServer(key);

  return RunUninstallScript(true);
}

// Uninstalls this version of the updater, without uninstalling any other
// versions. This version is assumed to not be the active version.
int UninstallCandidate(UpdaterScope scope) {
  {
    auto scoped_com_initializer =
        std::make_unique<base::win::ScopedCOMInitializer>(
            base::win::ScopedCOMInitializer::kMTA);
    updater::UnregisterWakeTask();
  }

  // TODO(crbug.com/1175095): Remove the UpdateServiceInternal server as well.
  // TODO(crbug.com/1175095): Remove COM interfaces.

  return RunUninstallScript(false);
}

}  // namespace updater
