// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server_win.h"

#include <regstr.h>
#include <wrl/module.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected_macros.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/app/server/win/update_service_internal_stub_win.h"
#include "chrome/updater/app/server/win/update_service_stub_win.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/setup/uninstall.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {
namespace {

std::wstring GetCOMGroup(const std::wstring& prefix, UpdaterScope scope) {
  return base::StrCat({prefix, base::ASCIIToWide(UpdaterScopeToString(scope))});
}

std::wstring COMGroup(UpdaterScope scope) {
  return GetCOMGroup(L"Active", scope);
}

std::wstring COMGroupInternal(UpdaterScope scope) {
  return GetCOMGroup(L"Internal", scope);
}

HRESULT AddAllowedAce(HANDLE object,
                      SE_OBJECT_TYPE object_type,
                      const CSid& sid,
                      ACCESS_MASK required_permissions,
                      UINT8 required_ace_flags) {
  CDacl dacl;
  if (!AtlGetDacl(object, object_type, &dacl)) {
    return HRESULTFromLastError();
  }

  int ace_count = dacl.GetAceCount();
  for (int i = 0; i < ace_count; ++i) {
    CSid sid_entry;
    ACCESS_MASK existing_permissions = 0;
    BYTE existing_ace_flags = 0;
    dacl.GetAclEntry(i, &sid_entry, &existing_permissions, NULL,
                     &existing_ace_flags);
    if (sid_entry == sid &&
        required_permissions == (existing_permissions & required_permissions) &&
        required_ace_flags == (existing_ace_flags & ~INHERITED_ACE)) {
      return S_OK;
    }
  }

  if (!dacl.AddAllowedAce(sid, required_permissions, required_ace_flags) ||
      !AtlSetDacl(object, object_type, dacl)) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

bool CreateClientStateMedium() {
  base::win::RegKey key;
  LONG result = key.Create(HKEY_LOCAL_MACHINE, CLIENT_STATE_MEDIUM_KEY,
                           Wow6432(KEY_WRITE));
  if (result != ERROR_SUCCESS) {
    VLOG(2) << __func__ << " failed: CreateKey returned " << result;
    return false;
  }
  // Authenticated non-admins may read, write, create subkeys and values.
  // The override privileges apply to all subkeys and values but not to the
  // ClientStateMedium key itself.
  HRESULT hr = AddAllowedAce(
      key.Handle(), SE_REGISTRY_KEY, Sids::Interactive(),
      KEY_READ | KEY_SET_VALUE | KEY_CREATE_SUB_KEY,
      CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE);
  if (FAILED(hr)) {
    VLOG(2) << __func__ << " failed: AddAllowedAce returned " << hr;
    return false;
  }
  return true;
}

// Install Updater.exe as GoogleUpdate.exe in the file system under
// Google\Update. And add a "pv" registry value under the
// UPDATER_KEY\Clients\{GoogleUpdateAppId}.
// Finally, update the registry value for the "UninstallCmdLine".
bool SwapGoogleUpdate(UpdaterScope scope,
                      const base::FilePath& updater_path,
                      const base::FilePath& temp_path,
                      HKEY root,
                      WorkItemList* list) {
  CHECK(list);

  const std::optional<base::FilePath> target_path =
      GetGoogleUpdateExePath(scope);
  if (!target_path || !base::CreateDirectory(target_path->DirName())) {
    return false;
  }
  list->AddCopyTreeWorkItem(updater_path, *target_path, temp_path,
                            WorkItem::ALWAYS);

  const std::wstring google_update_appid_key =
      GetAppClientsKey(kLegacyGoogleUpdateAppID);
  list->AddCreateRegKeyWorkItem(root, COMPANY_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, UPDATER_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, CLIENTS_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, CLIENT_STATE_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, google_update_appid_key, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(
      root, GetAppClientStateKey(kLegacyGoogleUpdateAppID), KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(root, google_update_appid_key, KEY_WOW64_32KEY,
                               kRegValuePV, kUpdaterVersionUtf16, true);
  list->AddSetRegValueWorkItem(
      root, GetAppClientStateKey(kLegacyGoogleUpdateAppID), KEY_WOW64_32KEY,
      kRegValuePV, kUpdaterVersionUtf16, true);
  list->AddSetRegValueWorkItem(
      root, google_update_appid_key, KEY_WOW64_32KEY, kRegValueName,
      base::ASCIIToWide(PRODUCT_FULLNAME_STRING), true);
  list->AddSetRegValueWorkItem(
      root, UPDATER_KEY, KEY_WOW64_32KEY, kRegValueUninstallCmdLine,
      [scope, &updater_path] {
        base::CommandLine uninstall_if_unused_command(updater_path);
        uninstall_if_unused_command.AppendSwitch(kWakeSwitch);
        if (IsSystemInstall(scope)) {
          uninstall_if_unused_command.AppendSwitch(kSystemSwitch);
        }
        return uninstall_if_unused_command.GetCommandLineString();
      }(),
      true);
  list->AddSetRegValueWorkItem(root, UPDATER_KEY, KEY_WOW64_32KEY,
                               kRegValueVersion, kUpdaterVersionUtf16, true);
  return true;
}

// Uninstall the GoogleUpdate services, run values, scheduled tasks, and files.
bool UninstallGoogleUpdate(UpdaterScope scope) {
  VLOG(2) << __func__;

  if (IsSystemInstall(scope)) {
    // Delete the GoogleUpdate services.
    ForEachServiceWithPrefix(
        kLegacyServiceNamePrefix, kLegacyServiceDisplayNamePrefix,
        [](const std::wstring& service_name) {
          VLOG(2) << __func__ << ": Deleting legacy service: " << service_name;
          if (!DeleteService(service_name)) {
            VLOG(1) << __func__
                    << ": failed to delete service: " << service_name;
          }
        });
  } else {
    // Delete the GoogleUpdate run values.
    ForEachRegistryRunValueWithPrefix(
        kLegacyRunValuePrefix, [](const std::wstring& run_name) {
          VLOG(2) << __func__ << ": Deleting legacy run value: " << run_name;
          base::win::RegKey(HKEY_CURRENT_USER, REGSTR_PATH_RUN, KEY_WRITE)
              .DeleteValue(run_name.c_str());
        });
  }

  // Delete the GoogleUpdate tasks. This is a best-effort operation.
  scoped_refptr<TaskScheduler> task_scheduler(
      TaskScheduler::CreateInstance(scope, /*use_task_subfolders=*/false));
  if (task_scheduler) {
    task_scheduler->ForEachTaskWithPrefix(
        IsSystemInstall(scope) ? kLegacyTaskNamePrefixSystem
                               : kLegacyTaskNamePrefixUser,
        [&task_scheduler](const std::wstring& task_name) {
          VLOG(2) << __func__ << ": Deleting legacy task: " << task_name;
          bool is_deleted = task_scheduler->DeleteTask(task_name);
          VLOG_IF(2, !is_deleted) << "Legacy task was not deleted.";
        });
  }

  // Keep only `GoogleUpdate.exe` and nothing else under `\Google\Update`.
  return DeleteExcept(GetGoogleUpdateExePath(scope));
}

}  // namespace

HRESULT IsCOMCallerAllowed() {
  if (!IsSystemInstall()) {
    return S_OK;
  }

  ASSIGN_OR_RETURN(const bool result, IsCOMCallerAdmin(), [](HRESULT error) {
    LOG(ERROR) << "IsCOMCallerAdmin failed: " << std::hex << error;
    return error;
  });

  return result ? S_OK : E_ACCESSDENIED;
}

scoped_refptr<App> MakeAppServer() {
  return GetAppServerWinInstance();
}

// Returns a leaky singleton instance of `AppServerWin`.
scoped_refptr<AppServerWin> GetAppServerWinInstance() {
  static base::NoDestructor<scoped_refptr<AppServerWin>> app_server{
      base::MakeRefCounted<AppServerWin>()};
  return *app_server;
}

AppServerWin::AppServerWin() = default;
AppServerWin::~AppServerWin() {
  NOTREACHED_IN_MIGRATION();  // The instance of this class is a leaky
                              // singleton.
}

void AppServerWin::PostRpcTask(base::OnceClosure task) {
  GetAppServerWinInstance()->PostRpcTaskOnMainSequence(std::move(task));
}

void AppServerWin::Stop() {
  VLOG(2) << __func__ << ": COM server is shutting down.";
  UnregisterClassObjects();
  main_task_runner_->PostTask(FROM_HERE, base::BindOnce([] {
                                scoped_refptr<AppServerWin> this_server =
                                    GetAppServerWinInstance();
                                this_server->update_service_ = nullptr;
                                this_server->update_service_internal_ = nullptr;
                                this_server->Shutdown(0);
                              }));
}

void AppServerWin::PostRpcTaskOnMainSequence(base::OnceClosure task) {
  main_task_runner_->PostTask(FROM_HERE, std::move(task));
}

HRESULT AppServerWin::RegisterClassObjects() {
  // Register COM class objects that are under either the ActiveSystem or the
  // ActiveUser group.
  // See wrl_classes.cc for details on the COM classes within the group.
  return Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
      .RegisterObjects(COMGroup(updater_scope()).c_str());
}

HRESULT AppServerWin::RegisterInternalClassObjects() {
  // Register COM class objects that are under either the InternalSystem or the
  // InternalUser group.
  // See wrl_classes.cc for details on the COM classes within the group.
  return Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
      .RegisterObjects(COMGroupInternal(updater_scope()).c_str());
}

void AppServerWin::UnregisterClassObjects() {
  const HRESULT hr =
      Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
          .UnregisterObjects();
  LOG_IF(ERROR, FAILED(hr)) << "UnregisterObjects failed; hr: " << hr;
}

void AppServerWin::CreateWRLModule() {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(
      this, &AppServerWin::Stop);
}

void AppServerWin::TaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto count =
      Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
          .IncrementObjectCount();
  VLOG(2) << "Starting task, Microsoft::WRL::Module count: " << count;
  AppServer::TaskStarted();
}

bool AppServerWin::ShutdownIfIdleAfterTask() {
  return false;
}

void AppServerWin::OnDelayedTaskComplete() {
  const auto count =
      Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
          .DecrementObjectCount();
  VLOG(2) << "Completed task, Microsoft::WRL::Module count: " << count;
}

void AppServerWin::ActiveDuty(scoped_refptr<UpdateService> update_service) {
  update_service_ = base::MakeRefCounted<UpdateServiceStubWin>(
      std::move(update_service),
      base::BindRepeating(&AppServerWin::TaskStarted, this),
      base::BindRepeating(&AppServerWin::TaskCompleted, this));
  Start(base::BindOnce(&AppServerWin::RegisterClassObjects,
                       base::Unretained(this)));
}

void AppServerWin::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  update_service_internal_ = base::MakeRefCounted<UpdateServiceInternalStubWin>(
      std::move(update_service_internal),
      base::BindRepeating(&AppServerWin::TaskStarted, this),
      base::BindRepeating(&AppServerWin::TaskCompleted, this));
  Start(base::BindOnce(&AppServerWin::RegisterInternalClassObjects,
                       base::Unretained(this)));
}

void AppServerWin::Start(base::OnceCallback<HRESULT()> register_callback) {
  CreateWRLModule();
  HRESULT hr = std::move(register_callback).Run();
  if (FAILED(hr)) {
    Shutdown(hr);
  }
}

void AppServerWin::UninstallSelf() {
  UninstallCandidate(updater_scope());
}

bool AppServerWin::SwapInNewVersion() {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());

  const std::optional<base::FilePath> versioned_directory =
      GetVersionedInstallDirectory(updater_scope());
  if (!versioned_directory) {
    return false;
  }

  const base::FilePath updater_path =
      versioned_directory->Append(GetExecutableRelativePath());

  if (IsSystemInstall(updater_scope()) && !CreateClientStateMedium()) {
    return false;
  }

  std::optional<base::ScopedTempDir> temp_dir = CreateSecureTempDir();
  if (!temp_dir) {
    return false;
  }

  if (!SwapGoogleUpdate(updater_scope(), updater_path, temp_dir->GetPath(),
                        UpdaterScopeToHKeyRoot(updater_scope()), list.get())) {
    return false;
  }

  if (IsSystemInstall(updater_scope())) {
    AddComServiceWorkItems(updater_path, false, list.get());
  } else {
    AddComServerWorkItems(updater_path, false, list.get());
  }

  const base::ScopedClosureRunner reset_shutdown_event(
      SignalShutdownEvent(updater_scope()));

  std::optional<base::FilePath> target =
      GetGoogleUpdateExePath(updater_scope());
  if (target) {
    StopProcessesUnderPath(target->DirName(), base::Seconds(45));
  }

  if (!list->Do()) {
    return false;
  }

  if (!UninstallGoogleUpdate(updater_scope())) {
    VLOG(1) << "UninstallGoogleUpdate() failed.";
  }

  if (!IsSystemInstall(updater_scope())) {
    if (!DeleteLegacyEntriesPerUser()) {
      VLOG(1) << "DeleteLegacyEntriesPerUser() failed.";
    }
  }

  return true;
}

void AppServerWin::RepairUpdater(UpdaterScope scope, bool is_internal) {
  if (AreComInterfacesPresent(scope, is_internal)) {
    return;
  }
  InstallComInterfaces(scope, is_internal);
}

}  // namespace updater
