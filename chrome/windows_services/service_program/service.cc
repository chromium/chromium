// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/service.h"

#include <sddl.h>
#include <wrl/module.h>

#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/win/atl.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/windows_services/service_program/process_wrl_module.h"
#include "chrome/windows_services/service_program/service_delegate.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace {

Service* g_instance = nullptr;

// Command line switch "--console" runs the service interactively for debugging
// purposes.
constexpr std::string_view kConsoleSwitchName = "console";

// NOTE: this value is ignored because service type is SERVICE_WIN32_OWN_PROCESS
// please see
// https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_table_entrya#members
constexpr wchar_t kWindowsServiceName[] = L"";

}  // namespace

Service::Service(ServiceDelegate& delegate) : delegate_(delegate) {
  CHECK_EQ(std::exchange(g_instance, this), nullptr);
}

Service::~Service() {
  CHECK_EQ(std::exchange(g_instance, nullptr), this);
}

bool Service::InitWithCommandLine(const base::CommandLine* command_line) {
  const base::CommandLine::StringVector args = command_line->GetArgs();
  if (!args.empty()) {
    LOG(ERROR) << "No positional parameters expected.";
    return false;
  }

  // Run interactively if needed.
  if (command_line->HasSwitch(kConsoleSwitchName)) {
    run_routine_ = &Service::RunInteractive;
  }

  // Register an empty FeatureList so that queries on it do not fail.
  base::FeatureList::SetInstance(std::make_unique<base::FeatureList>());

  return true;
}

// Start() is the entry point called by ServiceMain().
int Service::Start() {
  return (this->*run_routine_)();
}

// When _Service gets called, it initializes COM, and then calls Run().
// Run() initializes security, then calls RegisterClassObjects().
HRESULT Service::RegisterClassObjects() {
  auto& module = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();

  // We hand-register the class factories to support unique CLSIDs for each
  // Chrome channel, which is determined at runtime.
  auto factories_or_error = delegate_->CreateClassFactories();
  if (!factories_or_error.has_value()) {
    LOG(ERROR) << "Factory creation failed; hr: " << factories_or_error.error();
    return factories_or_error.error();
  }
  base::HeapArray<FactoryAndClsid>& factories = factories_or_error.value();

  // An array to hold unowned pointers to the IClassFactory interfaces. These
  // must not be Released.
  auto weak_factories =
      base::HeapArray<IClassFactory*>::Uninit(factories.size());
  // An array to hold the CLSIDs for each factory.
  auto class_ids = base::HeapArray<IID>::Uninit(factories.size());
  // An array to hold the registration cookie for each factory.
  cookies_ = base::HeapArray<DWORD>::Uninit(factories.size());

  size_t i = 0;
  for (auto& factory_and_clsid : factories) {
    weak_factories[i] = factory_and_clsid.factory.Get();
    class_ids[i] = factory_and_clsid.clsid;
    ++i;
  }

  HRESULT hr = module.RegisterCOMObject(
      nullptr, class_ids.data(), weak_factories.data(), cookies_.data(),
      static_cast<unsigned int>(factories.size()));
  if (FAILED(hr)) {
    LOG(ERROR) << "RegisterCOMObject failed; hr: " << hr;
    return hr;
  }

  // Register a callback with the process's WRL module to signal the service to
  // exit when the last reference is released.
  SetModuleReleasedCallback(
      base::BindOnce(&Service::SignalExit, base::Unretained(this)));

  return hr;
}

void Service::UnregisterClassObjects() {
  // Clear the callback registered with the process's WRL module.
  SetModuleReleasedCallback({});

  auto& module = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();
  const HRESULT hr =
      module.UnregisterCOMObject(nullptr, cookies_.data(), cookies_.size());
  if (FAILED(hr)) {
    LOG(ERROR) << "UnregisterCOMObject failed; hr: " << hr;
  }
}

Service& Service::GetInstance() {
  return CHECK_DEREF(g_instance);
}

int Service::RunAsService() {
  static constexpr SERVICE_TABLE_ENTRY dispatch_table[] = {
      {const_cast<LPTSTR>(kWindowsServiceName), &Service::ServiceMainEntry},
      {nullptr, nullptr}};

  if (!::StartServiceCtrlDispatcher(dispatch_table)) {
    service_status_.dwWin32ExitCode = ::GetLastError();
    PLOG(ERROR) << "Failed to connect to the service control manager";
  }

  return service_status_.dwWin32ExitCode;
}

void Service::ServiceMainImpl() {
  service_status_handle_ = ::RegisterServiceCtrlHandler(
      kWindowsServiceName, &Service::ServiceControlHandler);
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
    PLOG(ERROR) << "Failed to initialize COM";
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

int Service::RunInteractive() {
  return Run();
}

// static
void Service::ServiceControlHandler(DWORD control) {
  Service& self = Service::GetInstance();
  switch (control) {
    case SERVICE_CONTROL_STOP:
      self.SetServiceStatus(SERVICE_STOP_PENDING);
      self.SignalExit();
      break;

    default:
      break;
  }
}

// static
void WINAPI Service::ServiceMainEntry(DWORD argc, wchar_t* argv[]) {
  GetInstance().ServiceMainImpl();
}

void Service::SetServiceStatus(DWORD state) {
  ::InterlockedExchange(&service_status_.dwCurrentState, state);
  ::SetServiceStatus(service_status_handle_, &service_status_);
}

HRESULT Service::Run() {
  // Allow this thread to run tasks. Consider using a UI message pump if there
  // is a need to respond to window messages (e.g., WM_ENDSESSION).
  base::SingleThreadTaskExecutor task_executor;
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitWhenIdleClosure();

  delegate_->PreRun();
  absl::Cleanup post_run = [&delegate = *delegate_] { delegate.PostRun(); };

  HRESULT hr = InitializeComSecurity();
  if (FAILED(hr)) {
    return hr;
  }

  hr = RegisterClassObjects();
  if (SUCCEEDED(hr)) {
    run_loop.Run();
    UnregisterClassObjects();
  }

  return hr;
}

// static
HRESULT Service::InitializeComSecurity() {
  CDacl dacl;
  constexpr auto com_rights_execute_local =
      COM_RIGHTS_EXECUTE | COM_RIGHTS_EXECUTE_LOCAL;
  if (!dacl.AddAllowedAce(Sids::System(), com_rights_execute_local) ||
      !dacl.AddAllowedAce(Sids::Admins(), com_rights_execute_local) ||
      !dacl.AddAllowedAce(Sids::Interactive(), com_rights_execute_local)) {
    return E_ACCESSDENIED;
  }

  CSecurityDesc sd;
  sd.SetDacl(dacl);
  sd.MakeAbsolute();
  sd.SetOwner(Sids::Admins());
  sd.SetGroup(Sids::Admins());

  // These are the flags being set:
  // EOAC_DYNAMIC_CLOAKING: DCOM uses the thread token (if present) when
  //   determining the client's identity. Useful when impersonating another
  //   user.
  // EOAC_SECURE_REFS: Authenticates distributed reference count calls to
  //   prevent malicious users from releasing objects that are still being used.
  // EOAC_DISABLE_AAA: Causes any activation where a server process would be
  //   launched under the caller's identity (activate-as-activator) to fail with
  //   E_ACCESSDENIED.
  // EOAC_NO_CUSTOM_MARSHAL: reduces the chances of executing arbitrary DLLs
  //   because it allows the marshaling of only CLSIDs that are implemented in
  //   Ole32.dll, ComAdmin.dll, ComSvcs.dll, or Es.dll, or that implement the
  //   CATID_MARSHALER category ID.
  // RPC_C_AUTHN_LEVEL_PKT_PRIVACY: prevents replay attacks, verifies that none
  //   of the data transferred between the client and server has been modified,
  //   ensures that the data transferred can only be seen unencrypted by the
  //   client and the server.
  return ::CoInitializeSecurity(
      const_cast<SECURITY_DESCRIPTOR*>(sd.GetPSECURITY_DESCRIPTOR()), -1,
      nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IDENTIFY,
      nullptr,
      EOAC_DYNAMIC_CLOAKING | EOAC_DISABLE_AAA | EOAC_SECURE_REFS |
          EOAC_NO_CUSTOM_MARSHAL,
      nullptr);
}

void Service::SignalExit() {
  if (quit_closure_) {
    quit_closure_.Run();
  }
}
