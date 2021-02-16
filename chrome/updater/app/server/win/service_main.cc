// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/service_main.h"

#include <atlsecurity.h>
#include <sddl.h>

#include <type_traits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/app/server/win/com_classes.h"
#include "chrome/updater/app/server/win/com_classes_legacy.h"
#include "chrome/updater/app/server/win/server.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/wrl_module.h"

namespace updater {

namespace {

// Command line switch "--console" runs the service interactively for
// debugging purposes.
constexpr char kConsoleSwitchName[] = "console";

}  // namespace

int ServiceMain::RunComService(const base::CommandLine* command_line) {
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    LOG(ERROR) << "Failed to initialize COM";
    return CO_E_INITIALIZATIONFAILED;
  }

  // Run the COM service.
  ServiceMain* service = ServiceMain::GetInstance();
  if (!service->InitWithCommandLine(command_line))
    return ERROR_BAD_ARGUMENTS;

  int ret = service->Start();
  DCHECK_NE(ret, int{STILL_ACTIVE});
  return ret;
}

ServiceMain* ServiceMain::GetInstance() {
  static base::NoDestructor<ServiceMain> instance;
  return instance.get();
}

bool ServiceMain::InitWithCommandLine(const base::CommandLine* command_line) {
  const base::CommandLine::StringVector args = command_line->GetArgs();
  if (!args.empty()) {
    LOG(ERROR) << "No positional parameters expected.";
    return false;
  }

  // Run interactively if needed.
  if (command_line->HasSwitch(kConsoleSwitchName))
    run_routine_ = &ServiceMain::RunInteractive;

  return true;
}

// Start() is the entry point called by WinMain.
int ServiceMain::Start() {
  return (this->*run_routine_)();
}

ServiceMain::ServiceMain()
    : exit_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {
  service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status_.dwCurrentState = SERVICE_STOPPED;
  service_status_.dwControlsAccepted = SERVICE_ACCEPT_STOP;
}

ServiceMain::~ServiceMain() = default;

void ServiceMain::CreateWRLModule() {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(
      this, &ServiceMain::SignalExit);
}

// When _ServiceMain gets called, it initializes COM, and then calls Run().
// Run() initializes security, then calls RegisterClassObject().
HRESULT ServiceMain::RegisterClassObject() {
  auto& module = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();

  Microsoft::WRL::ComPtr<IUnknown> factory;
  unsigned int flags = Microsoft::WRL::ModuleType::OutOfProc;

  HRESULT hr = Microsoft::WRL::Details::CreateClassFactory<
      Microsoft::WRL::SimpleClassFactory<UpdaterImpl>>(
      &flags, nullptr, __uuidof(IClassFactory), &factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "Factory creation failed; hr: " << hr;
    return hr;
  }

  Microsoft::WRL::ComPtr<IClassFactory> class_factory;
  hr = factory.As(&class_factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "IClassFactory object creation failed; hr: " << hr;
    return hr;
  }

  // The pointer in this array is unowned. Do not release it.
  IClassFactory* class_factories[] = {class_factory.Get()};
  static_assert(
      std::extent<decltype(cookies_)>() == base::size(class_factories),
      "Arrays cookies_ and class_factories must be the same size.");

  IID class_ids[] = {__uuidof(UpdaterClass)};
  DCHECK_EQ(base::size(cookies_), base::size(class_ids));
  static_assert(std::extent<decltype(cookies_)>() == base::size(class_ids),
                "Arrays cookies_ and class_ids must be the same size.");

  hr = module.RegisterCOMObject(nullptr, class_ids, class_factories, cookies_,
                                base::size(cookies_));
  if (FAILED(hr)) {
    LOG(ERROR) << "RegisterCOMObject failed; hr: " << hr;
    return hr;
  }

  return hr;
}

void ServiceMain::UnregisterClassObject() {
  auto& module = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();
  const HRESULT hr =
      module.UnregisterCOMObject(nullptr, cookies_, base::size(cookies_));
  LOG_IF(ERROR, FAILED(hr)) << "UnregisterCOMObject failed; hr: " << hr;
}

int ServiceMain::RunAsService() {
  static constexpr SERVICE_TABLE_ENTRY dispatch_table[] = {
      {const_cast<LPTSTR>(kWindowsServiceName), &ServiceMain::ServiceMainEntry},
      {nullptr, nullptr}};

  if (!::StartServiceCtrlDispatcher(dispatch_table)) {
    service_status_.dwWin32ExitCode = ::GetLastError();
    PLOG(ERROR) << "Failed to connect to the service control manager";
  }

  return service_status_.dwWin32ExitCode;
}

void ServiceMain::ServiceMainImpl() {
  service_status_handle_ = ::RegisterServiceCtrlHandler(
      kWindowsServiceName, &ServiceMain::ServiceControlHandler);
  if (service_status_handle_ == nullptr) {
    PLOG(ERROR) << "RegisterServiceCtrlHandler failed";
    return;
  }
  SetServiceStatus(SERVICE_RUNNING);

  service_status_.dwWin32ExitCode = ERROR_SUCCESS;
  service_status_.dwCheckPoint = 0;
  service_status_.dwWaitHint = 0;

  // Initialize COM for the current thread.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    LOG(ERROR) << "Failed to initialize COM";
    SetServiceStatus(SERVICE_STOPPED);
    return;
  }

  // When the Run function returns, the service has stopped.
  const HRESULT hr = Run();
  if (FAILED(hr)) {
    service_status_.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
    service_status_.dwServiceSpecificExitCode = hr;
  }

  SetServiceStatus(SERVICE_STOPPED);
}

int ServiceMain::RunInteractive() {
  return Run();
}

// static
void ServiceMain::ServiceControlHandler(DWORD control) {
  ServiceMain* self = ServiceMain::GetInstance();
  switch (control) {
    case SERVICE_CONTROL_STOP:
      self->SetServiceStatus(SERVICE_STOP_PENDING);
      self->SignalExit();
      break;

    default:
      break;
  }
}

// static
void WINAPI ServiceMain::ServiceMainEntry(DWORD argc, wchar_t* argv[]) {
  ServiceMain::GetInstance()->ServiceMainImpl();
}

void ServiceMain::SetServiceStatus(DWORD state) {
  service_status_.dwCurrentState = state;
  ::SetServiceStatus(service_status_handle_, &service_status_);
}

HRESULT ServiceMain::Run() {
  HRESULT hr = InitializeComSecurity();
  if (FAILED(hr))
    return hr;

  CreateWRLModule();
  hr = RegisterClassObject();
  if (SUCCEEDED(hr)) {
    WaitForExitSignal();
    UnregisterClassObject();
  }

  return hr;
}

// static
HRESULT ServiceMain::InitializeComSecurity() {
  CDacl dacl;
  constexpr auto com_rights_execute_local =
      COM_RIGHTS_EXECUTE | COM_RIGHTS_EXECUTE_LOCAL;
  dacl.AddAllowedAce(Sids::System(), com_rights_execute_local);
  dacl.AddAllowedAce(Sids::Admins(), com_rights_execute_local);
  dacl.AddAllowedAce(Sids::Interactive(), com_rights_execute_local);

  CSecurityDesc sd;
  sd.SetDacl(dacl);
  sd.MakeAbsolute();
  sd.SetOwner(Sids::Admins());
  sd.SetGroup(Sids::Admins());

  return ::CoInitializeSecurity(
      const_cast<SECURITY_DESCRIPTOR*>(sd.GetPSECURITY_DESCRIPTOR()), -1,
      nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IDENTIFY,
      nullptr, EOAC_DYNAMIC_CLOAKING | EOAC_NO_CUSTOM_MARSHAL, nullptr);
}

void ServiceMain::WaitForExitSignal() {
  exit_signal_.Wait();
}

void ServiceMain::SignalExit() {
  exit_signal_.Signal();
}

}  // namespace updater
