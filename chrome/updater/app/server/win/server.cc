// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/server.h"

#include <wrl/module.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/app/server/win/com_classes.h"
#include "chrome/updater/app/server/win/com_classes_legacy.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/setup/uninstall.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {
namespace {

bool IsCOMService() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kComServiceSwitch);
}

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
  if (scope == UpdaterScope::kSystem)
    uninstall_if_unused_command.AppendSwitch(kSystemSwitch);
  uninstall_if_unused_command.AppendSwitch(kEnableLoggingSwitch);
  uninstall_if_unused_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                                kLoggingModuleSwitchValue);
  list->AddCreateRegKeyWorkItem(root, UPDATER_KEY, Wow6432(0));
  list->AddSetRegValueWorkItem(
      root, UPDATER_KEY, Wow6432(0), kRegValueUninstallCmdLine,
      uninstall_if_unused_command.GetCommandLineString(), true);

  return true;
}

}  // namespace

// Returns a leaky singleton of the App instance.
scoped_refptr<ComServerApp> AppServerSingletonInstance() {
  return AppSingletonInstance<ComServerApp>();
}

ComServerApp::ComServerApp()
    : com_initializer_(base::win::ScopedCOMInitializer::kMTA) {}

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

void ComServerApp::InitializeThreadPool() {
  base::ThreadPoolInstance::Create(kThreadPoolName);

  // Reuses the logic in base::ThreadPoolInstance::StartWithDefaultParams.
  const int num_cores = base::SysInfo::NumberOfProcessors();
  const int max_num_foreground_threads = std::max(3, num_cores - 1);
  base::ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
  init_params.common_thread_pool_environment = base::ThreadPoolInstance::
      InitParams::CommonThreadPoolEnvironment::COM_MTA;
  base::ThreadPoolInstance::Get()->Start(init_params);
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
  if (!com_initializer_.Succeeded()) {
    PLOG(ERROR) << "Failed to initialize COM";
    Shutdown(-1);
    return;
  }
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
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
      GetVersionedDirectory(updater_scope());
  if (!versioned_directory)
    return false;

  const base::FilePath updater_path =
      versioned_directory->Append(FILE_PATH_LITERAL("updater.exe"));

  HKEY root = (updater_scope() == UpdaterScope::kSystem) ? HKEY_LOCAL_MACHINE
                                                         : HKEY_CURRENT_USER;
  if (!SwapUninstallCmdLine(updater_scope(), updater_path, root, list.get()))
    return false;

  if (IsCOMService()) {
    AddComServiceWorkItems(updater_path, false, list.get());
  } else {
    for (const CLSID& clsid : GetActiveServers(updater_scope())) {
      AddInstallServerWorkItems(root, clsid, updater_path, false, list.get());
    }

    for (const GUID& iid : GetActiveInterfaces()) {
      AddInstallComInterfaceWorkItems(root, updater_path, iid, list.get());
    }
  }

  return list->Do();
}

bool ComServerApp::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  // TODO(crbug.com/1250524): Implement. Note we will need both user and system
  // scopes here.
  return true;
}

}  // namespace updater
