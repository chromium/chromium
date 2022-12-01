// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/server.h"

#include <wrl/module.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/app/server/win/com_classes.h"
#include "chrome/updater/app/server/win/com_classes_legacy.h"
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

// Update the registry value for the "UninstallCmdLine" under the UPDATER_KEY.
bool SwapUninstallCmdLine(UpdaterScope scope,
                          const base::FilePath& updater_path,
                          HKEY root,
                          WorkItemList* list) {
  DCHECK(list);

  base::CommandLine uninstall_if_unused_command(updater_path);

  // TODO(crbug.com/1270520) - use a switch that can uninstall immediately if
  // unused, instead of requiring server starts.
  uninstall_if_unused_command.AppendSwitch(kUninstallIfUnusedSwitch);
  if (IsSystemInstall(scope))
    uninstall_if_unused_command.AppendSwitch(kSystemSwitch);
  uninstall_if_unused_command.AppendSwitch(kEnableLoggingSwitch);
  uninstall_if_unused_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                                kLoggingModuleSwitchValue);
  list->AddSetRegValueWorkItem(
      root, UPDATER_KEY, KEY_WOW64_32KEY, kRegValueUninstallCmdLine,
      uninstall_if_unused_command.GetCommandLineString(), true);

  return true;
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
  DCHECK(list);

  const absl::optional<base::FilePath> target_path =
      GetGoogleUpdateExePath(scope);
  if (!target_path)
    return false;
  list->AddCopyTreeWorkItem(updater_path, *target_path, temp_path,
                            WorkItem::ALWAYS);

  const std::wstring google_update_appid_key =
      GetAppClientsKey(L"{430FD4D0-B729-4F61-AA34-91526481799D}");
  list->AddCreateRegKeyWorkItem(root, COMPANY_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, UPDATER_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, CLIENTS_KEY, KEY_WOW64_32KEY);
  list->AddCreateRegKeyWorkItem(root, google_update_appid_key, KEY_WOW64_32KEY);
  list->AddSetRegValueWorkItem(root, google_update_appid_key, KEY_WOW64_32KEY,
                               kRegValuePV, kUpdaterVersionUtf16, true);
  list->AddSetRegValueWorkItem(
      root, google_update_appid_key, KEY_WOW64_32KEY, kRegValueName,
      base::ASCIIToWide(PRODUCT_FULLNAME_STRING), true);

  return SwapUninstallCmdLine(scope, updater_path, root, list);
}

}  // namespace

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

void ComServerApp::ActiveDuty(scoped_refptr<UpdateService> update_service) {
  update_service_ = update_service;
  Start(base::BindOnce(&ComServerApp::RegisterClassObjects,
                       base::Unretained(this)));
}

void ComServerApp::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  update_service_internal_ = update_service_internal;
  Start(base::BindOnce(&ComServerApp::RegisterInternalClassObjects,
                       base::Unretained(this)));
}

void ComServerApp::Start(base::OnceCallback<HRESULT()> register_callback) {
  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  CreateWRLModule();
  HRESULT hr = std::move(register_callback).Run();
  if (FAILED(hr))
    Shutdown(hr);
}

void ComServerApp::UninstallSelf() {
  UninstallCandidate(updater_scope());
}

bool ComServerApp::SwapInNewVersion() {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());

  const absl::optional<base::FilePath> versioned_directory =
      GetVersionedDataDirectory(updater_scope());
  if (!versioned_directory)
    return false;

  const base::FilePath updater_path =
      versioned_directory->Append(GetExecutableRelativePath());

  if (IsSystemInstall(updater_scope()) && !CreateClientStateMedium()) {
    return false;
  }

  absl::optional<base::ScopedTempDir> temp_dir = CreateSecureTempDir();
  if (!temp_dir)
    return false;

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
  StopGoogleUpdateProcesses(updater_scope());

  return list->Do();
}

bool ComServerApp::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  const HKEY root = UpdaterScopeToHKeyRoot(updater_scope());
  for (base::win::RegistryKeyIterator it(root, CLIENTS_KEY, KEY_WOW64_32KEY);
       it.Valid(); ++it) {
    const std::wstring app_id = it.Name();

    // Skip importing legacy updater.
    if (base::EqualsCaseInsensitiveASCII(app_id, kLegacyGoogleUpdaterAppID))
      continue;

    base::win::RegKey key;
    if (key.Open(root, GetAppClientsKey(app_id).c_str(), Wow6432(KEY_READ)) !=
        ERROR_SUCCESS) {
      continue;
    }

    RegistrationRequest registration;
    registration.app_id = base::SysWideToUTF8(app_id);
    std::wstring pv;
    if (key.ReadValue(kRegValuePV, &pv) != ERROR_SUCCESS)
      continue;

    registration.version = base::Version(base::SysWideToUTF8(pv));
    if (!registration.version.IsValid())
      continue;

    std::wstring brand_code;
    if (key.ReadValue(kRegValueBrandCode, &brand_code) == ERROR_SUCCESS)
      registration.brand_code = base::SysWideToUTF8(brand_code);

    std::wstring ap;
    if (key.ReadValue(kRegValueAP, &ap) == ERROR_SUCCESS)
      registration.ap = base::SysWideToUTF8(ap);

    register_callback.Run(registration);
  }

  return true;
}

}  // namespace updater
