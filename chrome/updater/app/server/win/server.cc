// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/server.h"

#include <wrl/implements.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/app/server/win/com_classes.h"
#include "chrome/updater/app/server/win/com_classes_legacy.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/setup/uninstall.h"
#include "chrome/updater/win/wrl_module.h"
#include "components/prefs/pref_service.h"

namespace updater {

// Returns a leaky singleton of the App instance.
scoped_refptr<ComServerApp> AppServerSingletonInstance() {
  return AppSingletonInstance<ComServerApp>();
}

ComServerApp::ComServerApp()
    : com_initializer_(base::win::ScopedCOMInitializer::kMTA) {}

ComServerApp::~ComServerApp() = default;

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
  Microsoft::WRL::ComPtr<IUnknown> factory;
  unsigned int flags = Microsoft::WRL::ModuleType::OutOfProc;

  HRESULT hr = Microsoft::WRL::Details::CreateClassFactory<
      Microsoft::WRL::SimpleClassFactory<UpdaterImpl>>(
      &flags, nullptr, __uuidof(IClassFactory), &factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "Factory creation for UpdaterImpl failed; hr: " << hr;
    return hr;
  }

  Microsoft::WRL::ComPtr<IClassFactory> class_factory_updater;
  hr = factory.As(&class_factory_updater);
  if (FAILED(hr)) {
    LOG(ERROR) << "IClassFactory object creation failed; hr: " << hr;
    return hr;
  }
  factory.Reset();

  hr = Microsoft::WRL::Details::CreateClassFactory<
      Microsoft::WRL::SimpleClassFactory<LegacyOnDemandImpl>>(
      &flags, nullptr, __uuidof(IClassFactory), &factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "Factory creation for LegacyOnDemandImpl failed; hr: " << hr;
    return hr;
  }

  Microsoft::WRL::ComPtr<IClassFactory> class_factory_legacy_ondemand;
  hr = factory.As(&class_factory_legacy_ondemand);
  if (FAILED(hr)) {
    LOG(ERROR) << "IClassFactory object creation failed; hr: " << hr;
    return hr;
  }

  // The pointer in this array is unowned. Do not release it.
  IClassFactory* class_factories[] = {class_factory_updater.Get(),
                                      class_factory_legacy_ondemand.Get()};
  IID class_ids[] = {__uuidof(UpdaterClass),
                     __uuidof(GoogleUpdate3WebUserClass)};
  DWORD cookies[base::size(class_factories)];
  static_assert(std::extent<decltype(cookies)>() == base::size(class_ids),
                "Arrays cookies and class_ids must be the same size.");
  hr = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
           .RegisterCOMObject(nullptr, class_ids, class_factories, cookies,
                              base::size(cookies));
  for (DWORD cookie : cookies) {
    cookies_.push_back(cookie);
  }
  if (FAILED(hr)) {
    LOG(ERROR) << "RegisterCOMObject failed; hr: " << hr;
    return hr;
  }

  return hr;
}

HRESULT ComServerApp::RegisterInternalClassObjects() {
  Microsoft::WRL::ComPtr<IUnknown> factory;
  unsigned int flags = Microsoft::WRL::ModuleType::OutOfProc;

  HRESULT hr = Microsoft::WRL::Details::CreateClassFactory<
      Microsoft::WRL::SimpleClassFactory<UpdaterInternalImpl>>(
      &flags, nullptr, __uuidof(IClassFactory), &factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "Factory creation for UpdaterInternalImpl failed; hr: " << hr;
    return hr;
  }

  Microsoft::WRL::ComPtr<IClassFactory> class_factory_updater_internal;
  hr = factory.As(&class_factory_updater_internal);
  if (FAILED(hr)) {
    LOG(ERROR) << "IClassFactory object creation failed; hr: " << hr;
    return hr;
  }
  factory.Reset();

  // The pointer in this array is unowned. Do not release it.
  IClassFactory* class_factories[] = {class_factory_updater_internal.Get()};
  IID class_ids[] = {__uuidof(UpdaterInternalClass)};
  DWORD cookies[base::size(class_factories)];
  static_assert(std::extent<decltype(cookies)>() == base::size(class_ids),
                "Arrays cookies and class_ids must be the same size.");
  hr = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
           .RegisterCOMObject(nullptr, class_ids, class_factories, cookies,
                              base::size(cookies));
  for (DWORD cookie : cookies) {
    cookies_.push_back(cookie);
  }
  if (FAILED(hr)) {
    LOG(ERROR) << "RegisterCOMObject failed; hr: " << hr;
    return hr;
  }

  return hr;
}

void ComServerApp::UnregisterClassObjects() {
  auto& module = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();
  const HRESULT hr =
      module.UnregisterCOMObject(nullptr, cookies_.data(), cookies_.size());
  if (FAILED(hr))
    LOG(ERROR) << "UnregisterCOMObject failed; hr: " << hr;
}

void ComServerApp::CreateWRLModule() {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(
      this, &ComServerApp::Stop);
}

void ComServerApp::Stop() {
  VLOG(2) << __func__ << ": COM server is shutting down.";
  UnregisterClassObjects();
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce([]() {
        scoped_refptr<ComServerApp> this_server = AppServerSingletonInstance();
        this_server->update_service_ = nullptr;
        this_server->update_service_internal_ = nullptr;
        this_server->Shutdown(0);
      }));
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
  // TODO(crbug.com/1096654): Add support for UpdaterScope::kSystem.
  UninstallCandidate(UpdaterScope::kUser);
}

bool ComServerApp::SwapRPCInterfaces() {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());

  base::Optional<base::FilePath> versioned_directory = GetVersionedDirectory();
  if (!versioned_directory)
    return false;
  for (const CLSID& clsid : GetActiveServers()) {
    // TODO(crbug.com/1096654): Use HKLM for system.
    AddInstallServerWorkItems(
        HKEY_CURRENT_USER, clsid,
        versioned_directory->Append(FILE_PATH_LITERAL("updater.exe")), false,
        list.get());
  }

  // TODO(crbug.com/1096654): Add support for UpdaterScope::kSystem: A call to
  // AddComServiceWorkItems is needed.

  for (const GUID& iid : GetActiveInterfaces()) {
    // TODO(crbug.com/1096654): Use HKLM for system.
    AddInstallComInterfaceWorkItems(
        HKEY_CURRENT_USER,
        versioned_directory->Append(FILE_PATH_LITERAL("updater.exe")), iid,
        list.get());
  }

  return list->Do();
}

}  // namespace updater
