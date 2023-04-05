// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/server.h"

#include <regstr.h>
#include <wrl/module.h>

#include <memory>
#include <string>
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
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  const absl::optional<base::FilePath> target_path =
      GetGoogleUpdateExePath(scope);
  if (!target_path) {
    return false;
  }
  list->AddCopyTreeWorkItem(updater_path, *target_path, temp_path,
                            WorkItem::ALWAYS);

  const std::wstring google_update_appid_key =
      GetAppClientsKey(kLegacyGoogleUpdateAppID);
  list->AddCreateRegKeyWorkItem(root, COMPANY_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, UPDATER_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, CLIENTS_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, google_update_appid_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(root, google_update_appid_key, KEY_WOW64_32KEY,
                               kRegValuePV, kUpdaterVersionUtf16, true);
  list->AddSetRegValueWorkItem(
      root, google_update_appid_key, KEY_WOW64_32KEY, kRegValueName,
      base::ASCIIToWide(PRODUCT_FULLNAME_STRING), true);
  list->AddSetRegValueWorkItem(
      root, UPDATER_KEY, KEY_WOW64_32KEY, kRegValueUninstallCmdLine,
      [scope, &updater_path]() {
        base::CommandLine uninstall_if_unused_command(updater_path);
        uninstall_if_unused_command.AppendSwitch(kWakeSwitch);
        if (IsSystemInstall(scope)) {
          uninstall_if_unused_command.AppendSwitch(kSystemSwitch);
        }
        uninstall_if_unused_command.AppendSwitch(kEnableLoggingSwitch);
        uninstall_if_unused_command.AppendSwitchASCII(
            kLoggingModuleSwitch, kLoggingModuleSwitchValue);
        return uninstall_if_unused_command.GetCommandLineString();
      }(),
      true);
  list->AddSetRegValueWorkItem(root, UPDATER_KEY, KEY_WOW64_32KEY,
                               kRegValueVersion, kUpdaterVersionUtf16, true);
  return true;
}

// Uninstall the GoogleUpdate services, run values, scheduled tasks, and files.
bool UninstallGoogleUpdate(UpdaterScope scope,
                           const base::FilePath& temp_path,
                           HKEY root) {
  VLOG(2) << __func__;

  if (IsSystemInstall(scope)) {
    // Delete the GoogleUpdate services.
    ForEachServiceWithPrefix(
        kLegacyServiceNamePrefix, kLegacyServiceDisplayNamePrefix,
        base::BindRepeating([](const std::wstring& service_name) {
          VLOG(2) << __func__ << ": Deleting legacy service: " << service_name;
          if (!DeleteService(service_name)) {
            VLOG(1) << __func__
                    << ": failed to delete service: " << service_name;
          }
        }));
  } else {
    // Delete the GoogleUpdate run values.
    ForEachRegistryRunValueWithPrefix(
        kLegacyRunValuePrefix,
        base::BindRepeating([](const std::wstring& run_name) {
          VLOG(2) << __func__ << ": Deleting legacy run value: " << run_name;
          base::win::RegKey(HKEY_CURRENT_USER, REGSTR_PATH_RUN, KEY_WRITE)
              .DeleteValue(run_name.c_str());
        }));
  }

  // Delete the GoogleUpdate tasks.
  scoped_refptr<TaskScheduler> task_scheduler(
      TaskScheduler::CreateInstance(scope, /*use_task_subfolders=*/false));
  task_scheduler->ForEachTaskWithPrefix(
      IsSystemInstall(scope) ? kLegacyTaskNamePrefixSystem
                             : kLegacyTaskNamePrefixUser,
      base::BindRepeating(
          [](scoped_refptr<TaskScheduler> task_scheduler,
             const std::wstring& task_name) {
            VLOG(2) << __func__ << ": Deleting legacy task: " << task_name;
            task_scheduler->DeleteTask(task_name.c_str());
          },
          task_scheduler));

  // Keep only `GoogleUpdate.exe` and nothing else under `\Google\Update`.
  const absl::optional<base::FilePath> google_update_exe =
      GetGoogleUpdateExePath(scope);
  if (!google_update_exe) {
    return false;
  }

  base::FileEnumerator it(
      google_update_exe->DirName(), false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  for (base::FilePath name = it.Next(); !name.empty(); name = it.Next()) {
    if (name == google_update_exe) {
      continue;
    }

    VLOG(2) << __func__ << ": Deleting legacy path: " << name;
    list->AddDeleteTreeWorkItem(name, temp_path);
  }

  return list->Do();
}

absl::optional<int> DaynumFromDWORD(DWORD value) {
  const int daynum = static_cast<int>(value);

  // When daynum is positive, it is the number of days since January 1, 2007.
  // It's reasonable to only accept value between 3000 (maps to Mar 20, 2015)
  // and 50000 (maps to Nov 24, 2143).
  // -1 is special value for first install.
  return daynum == -1 || (daynum >= 3000 && daynum <= 50000)
             ? absl::make_optional(daynum)
             : absl::nullopt;
}

}  // namespace

HRESULT IsCOMCallerAllowed() {
  if (!IsSystemInstall()) {
    return S_OK;
  }

  HResultOr<bool> result = IsCOMCallerAdmin();
  if (!result.has_value()) {
    HRESULT hr = result.error();
    LOG(ERROR) << __func__ << ": IsCOMCallerAdmin failed: " << std::hex << hr;
    return hr;
  }

  return result.value() ? S_OK : E_ACCESSDENIED;
}

// Returns a leaky singleton of the App instance.
scoped_refptr<ComServerApp> AppServerSingletonInstance() {
  return AppSingletonInstance<ComServerApp>();
}

ComServerApp::ComServerApp() = default;
ComServerApp::~ComServerApp() = default;

void ComServerApp::Stop() {
  VLOG(2) << __func__ << ": COM server is shutting down.";
  UnregisterClassObjects();
  main_task_runner_->PostTask(FROM_HERE, base::BindOnce([]() {
                                scoped_refptr<ComServerApp> this_server =
                                    AppServerSingletonInstance();
                                this_server->update_service_ = nullptr;
                                this_server->update_service_internal_ = nullptr;
                                this_server->Shutdown(0);
                              }));
}

HRESULT ComServerApp::RegisterClassObjects() {
  // Register COM class objects that are under either the ActiveSystem or the
  // ActiveUser group.
  // See wrl_classes.cc for details on the COM classes within the group.
  return Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
      .RegisterObjects(COMGroup(updater_scope()).c_str());
}

HRESULT ComServerApp::RegisterInternalClassObjects() {
  // Register COM class objects that are under either the InternalSystem or the
  // InternalUser group.
  // See wrl_classes.cc for details on the COM classes within the group.
  return Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
      .RegisterObjects(COMGroupInternal(updater_scope()).c_str());
}

void ComServerApp::UnregisterClassObjects() {
  const HRESULT hr =
      Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
          .UnregisterObjects();
  LOG_IF(ERROR, FAILED(hr)) << "UnregisterObjects failed; hr: " << hr;
}

void ComServerApp::CreateWRLModule() {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(
      this, &ComServerApp::Stop);
}

void ComServerApp::TaskStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto count =
      Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
          .IncrementObjectCount();
  VLOG(2) << "Starting task, Microsoft::WRL::Module count: " << count;
}

void ComServerApp::TaskCompleted() {
  main_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&ComServerApp::AcknowledgeTaskCompletion, this),
      external_constants()->ServerKeepAliveTime());
}

void ComServerApp::AcknowledgeTaskCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto count =
      Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
          .DecrementObjectCount();
  VLOG(2) << "Completed task, Microsoft::WRL::Module count: " << count;
}

void ComServerApp::ActiveDuty(scoped_refptr<UpdateService> update_service) {
  update_service_ = base::MakeRefCounted<UpdateServiceStubWin>(
      std::move(update_service),
      base::BindRepeating(&ComServerApp::TaskStarted, this),
      base::BindRepeating(&ComServerApp::TaskCompleted, this));
  Start(base::BindOnce(&ComServerApp::RegisterClassObjects,
                       base::Unretained(this)));
}

void ComServerApp::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  update_service_internal_ = base::MakeRefCounted<UpdateServiceInternalStubWin>(
      std::move(update_service_internal),
      base::BindRepeating(&ComServerApp::TaskStarted, this),
      base::BindRepeating(&ComServerApp::TaskCompleted, this));
  Start(base::BindOnce(&ComServerApp::RegisterInternalClassObjects,
                       base::Unretained(this)));
}

void ComServerApp::Start(base::OnceCallback<HRESULT()> register_callback) {
  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  CreateWRLModule();
  HRESULT hr = std::move(register_callback).Run();
  if (FAILED(hr)) {
    Shutdown(hr);
  }
}

void ComServerApp::UninstallSelf() {
  UninstallCandidate(updater_scope());
}

bool ComServerApp::SwapInNewVersion() {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());

  const absl::optional<base::FilePath> versioned_directory =
      GetVersionedInstallDirectory(updater_scope());
  if (!versioned_directory) {
    return false;
  }

  const base::FilePath updater_path =
      versioned_directory->Append(GetExecutableRelativePath());

  if (IsSystemInstall(updater_scope()) && !CreateClientStateMedium()) {
    return false;
  }

  absl::optional<base::ScopedTempDir> temp_dir = CreateSecureTempDir();
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

  absl::optional<base::FilePath> target =
      GetGoogleUpdateExePath(updater_scope());
  if (target) {
    StopProcessesUnderPath(target->DirName(), base::Seconds(45));
  }

  const bool succeeded = list->Do();
  if (succeeded) {
    LOG_IF(ERROR,
           UninstallGoogleUpdate(updater_scope(), temp_dir->GetPath(),
                                 UpdaterScopeToHKeyRoot(updater_scope())));

    // TODO(crbug.com/1425609) - revert the CL that introduced this logging
    // after the bug is resolved.
    for (const auto& clsid : GetServers(false, updater_scope())) {
      LogClsidEntries(clsid);
    }
  }

  return succeeded;
}

bool ComServerApp::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  const HKEY root = UpdaterScopeToHKeyRoot(updater_scope());
  for (base::win::RegistryKeyIterator it(root, CLIENTS_KEY, KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    const std::wstring app_id = it.Name();

    // Skip importing legacy updater.
    if (base::EqualsCaseInsensitiveASCII(app_id, kLegacyGoogleUpdateAppID)) {
      continue;
    }

    base::win::RegKey key;
    if (key.Open(root, GetAppClientsKey(app_id).c_str(), Wow6432(KEY_READ)) !=
        ERROR_SUCCESS) {
      continue;
    }

    RegistrationRequest registration;
    registration.app_id = base::SysWideToUTF8(app_id);
    std::wstring pv;
    if (key.ReadValue(kRegValuePV, &pv) != ERROR_SUCCESS) {
      continue;
    }

    registration.version = base::Version(base::SysWideToUTF8(pv));
    if (!registration.version.IsValid()) {
      continue;
    }

    base::win::RegKey client_state_key;
    if (client_state_key.Open(root, GetAppClientStateKey(app_id).c_str(),
                              Wow6432(KEY_READ)) == ERROR_SUCCESS) {
      std::wstring brand_code;
      if (client_state_key.ReadValue(kRegValueBrandCode, &brand_code) ==
          ERROR_SUCCESS) {
        registration.brand_code = base::SysWideToUTF8(brand_code);
      }

      std::wstring ap;
      if (client_state_key.ReadValue(kRegValueAP, &ap) == ERROR_SUCCESS) {
        registration.ap = base::SysWideToUTF8(ap);
      }

      DWORD date_last_activity = 0;
      if (client_state_key.ReadValueDW(kRegValueDateOfLastActivity,
                                       &date_last_activity) == ERROR_SUCCESS) {
        registration.dla = DaynumFromDWORD(date_last_activity);
      }

      DWORD date_last_rollcall = 0;
      if (client_state_key.ReadValueDW(kRegValueDateOfLastRollcall,
                                       &date_last_rollcall) == ERROR_SUCCESS) {
        registration.dlrc = DaynumFromDWORD(date_last_rollcall);
      }
    }

    register_callback.Run(registration);
  }

  return true;
}

}  // namespace updater
