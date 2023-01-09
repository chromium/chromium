// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/service.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/credential_provider/extension/os_service_manager.h"
#include "chrome/credential_provider/extension/task_manager.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {
namespace extension {

// static
Service** Service::GetInstanceStorage() {
  static Service* instance = new Service();

  return &instance;
}

// static
Service* Service::Get() {
  return *GetInstanceStorage();
}

DWORD Service::Run() {
  return (this->*run_routine_)();
}

Service::Service()
    : run_routine_(&Service::RunAsService),
      service_status_(),
      service_status_handle_(nullptr) {
  service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status_.dwCurrentState = SERVICE_STOPPED;
  service_status_.dwControlsAccepted =
      SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN;
}

Service::~Service() {}

DWORD Service::RunAsService() {
  LOGFN(VERBOSE);

  DWORD error_code =
      extension::OSServiceManager::Get()->StartServiceCtrlDispatcher(
          &Service::ServiceMain);

  if (error_code != ERROR_SUCCESS) {
    LOGFN(ERROR)
        << "OSServiceManager::StartServiceCtrlDispatcher failed with win32="
        << error_code;
  }

  return error_code;
}

void Service::StartMain() {
  base::SingleThreadTaskExecutor main_task_executor;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
      main_task_executor.task_runner();

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  DWORD error_code = extension::OSServiceManager::Get()->RegisterCtrlHandler(
      &Service::ServiceControlHandler, &service_status_handle_);
  if (error_code != ERROR_SUCCESS) {
    LOGFN(ERROR) << "OSServiceManager::RegisterCtrlHandler failed win32="
                 << error_code;
    return;
  }

  service_status_.dwCurrentState = SERVICE_RUNNING;

  error_code = extension::OSServiceManager::Get()->SetServiceStatus(
      service_status_handle_, service_status_);
  if (error_code != ERROR_SUCCESS) {
    LOGFN(ERROR) << "OSServiceManager::SetServiceStatus failed win32="
                 << error_code;
    return;
  }

  TaskManager::Get()->RunTasks(main_task_runner);

  run_loop.Run();

  service_status_.dwCurrentState = SERVICE_STOPPED;
  service_status_.dwControlsAccepted = 0;

  error_code = extension::OSServiceManager::Get()->SetServiceStatus(
      service_status_handle_, service_status_);
  if (error_code != ERROR_SUCCESS)
    LOGFN(ERROR) << "OSServiceManager::SetServiceStatus failed win32="
                 << error_code;
}

// static
VOID WINAPI Service::ServiceMain(DWORD argc /*unused*/,
                                 WCHAR* argv[] /*unused*/) {
  LOGFN(VERBOSE);

  Service* self = Service::Get();

  // Run the service.
  self->StartMain();
}

// static
VOID WINAPI Service::ServiceControlHandler(DWORD control) {
  LOGFN(VERBOSE);

  Service* self = Service::Get();
  switch (control) {
    case SERVICE_CONTROL_PRESHUTDOWN:
    case SERVICE_CONTROL_STOP:
      self->service_status_.dwCurrentState = SERVICE_STOP_PENDING;

      extension::OSServiceManager::Get()->SetServiceStatus(
          self->service_status_handle_, self->service_status_);
      std::move(self->quit_closure_).Run();

      break;
    default:
      break;
  }
}

}  // namespace extension
}  // namespace credential_provider
