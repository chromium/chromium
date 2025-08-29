// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/service.h"

#include <objidl.h>
#include <sddl.h>
#include <wrl/module.h>

#include <algorithm>
#include <atomic>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/atl.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/windows_services/service_program/process_wrl_module.h"
#include "chrome/windows_services/service_program/service_delegate.h"

namespace {

std::atomic<Service*> g_instance(nullptr);

// Command line switch "--console" runs the service interactively for debugging
// purposes.
constexpr std::string_view kConsoleSwitchName = "console";

// NOTE: this value is ignored because service type is SERVICE_WIN32_OWN_PROCESS
// please see
// https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_table_entrya#members
constexpr wchar_t kWindowsServiceName[] = L"";

void WINAPI SpuriousServiceControlHandler(DWORD) {}

// A service main function that logs the state of its service and then stops it.
void HandleSpuriousServiceMain(DWORD argc, const wchar_t* const* argv) {
  using ServiceHandle =
      std::unique_ptr<SC_HANDLE__, decltype(&::CloseServiceHandle)>;

  // The first argument is the name of the service.
  const wchar_t* service_name = argc && *argv ? *argv : kWindowsServiceName;

  if (auto scm_raw = ::OpenSCManager(
          /*lpMachineName=*/nullptr,
          /*lpDatabaseName=*/SERVICES_ACTIVE_DATABASE, SC_MANAGER_CONNECT)) {
    ServiceHandle scm(scm_raw, &::CloseServiceHandle);
    if (auto svc_raw =
            ::OpenService(scm.get(), service_name, SERVICE_QUERY_STATUS)) {
      ServiceHandle svc(svc_raw, &::CloseServiceHandle);
      SERVICE_STATUS_PROCESS status = {};
      DWORD bytes_needed = 0;
      if (::QueryServiceStatusEx(svc.get(), SC_STATUS_PROCESS_INFO,
                                 reinterpret_cast<unsigned char*>(&status),
                                 sizeof(status), &bytes_needed)) {
        LOG(ERROR) << "Spurious start for " << service_name
                   << ". Current state: " << status.dwCurrentState
                   << "; pid: " << status.dwProcessId;
      } else {
        PLOG(ERROR) << "Failed to query " << service_name;
      }
    } else {
      PLOG(ERROR) << "Failed to open " << service_name;
    }
  } else {
    PLOG(ERROR) << "Failed to connect to SCM";
  }

  if (auto service_status_handle = ::RegisterServiceCtrlHandler(
          service_name, &SpuriousServiceControlHandler)) {
    SERVICE_STATUS service_status{.dwServiceType = SERVICE_WIN32_OWN_PROCESS,
                                  .dwCurrentState = SERVICE_STOPPED};
    ::SetServiceStatus(service_status_handle, &service_status);
  } else {
    PCHECK(false);
  }
}

}  // namespace

Service::Service(ServiceDelegate& delegate) : delegate_(delegate) {
  Service* expected = nullptr;
  CHECK(g_instance.compare_exchange_strong(expected, this,
                                           std::memory_order_relaxed));
}

Service::~Service() {
  Service* expected = this;
  CHECK(g_instance.compare_exchange_strong(expected, nullptr,
                                           std::memory_order_relaxed));
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
    exit_routine_ = &Service::StopInteractive;
  }

  return true;
}

// Start() is the entry point called by `ServiceProgramMain()`.
int Service::Start() {
  delegate_implements_run_ = delegate_->PreRun();

  const auto result = (this->*run_routine_)();

  delegate_->PostRun();

  return result;
}

// When _Service gets called, it initializes COM, and then calls Run().
// Run() initializes security, then calls RegisterClassObjects().
// static
HRESULT Service::RegisterClassObjects(ServiceDelegate& delegate,
                                      base::OnceClosure on_module_released,
                                      base::HeapArray<DWORD>& cookies) {
  auto& module = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();

  // We hand-register the class factories to support unique CLSIDs for each
  // Chrome channel, which is determined at runtime.
  auto factories_or_error = delegate.CreateClassFactories();
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
  auto new_cookies = base::HeapArray<DWORD>::Uninit(factories.size());

  size_t i = 0;
  for (auto& factory_and_clsid : factories) {
    weak_factories[i] = factory_and_clsid.factory.Get();
    class_ids[i] = factory_and_clsid.clsid;
    ++i;
  }

  // Register a callback with the process's WRL module to signal the service to
  // exit when the last reference is released.
  SetModuleReleasedCallback(std::move(on_module_released));

  HRESULT hr = module.RegisterCOMObject(
      nullptr, class_ids.data(), weak_factories.data(), new_cookies.data(),
      static_cast<unsigned int>(factories.size()));
  if (FAILED(hr)) {
    LOG(ERROR) << "RegisterCOMObject failed; hr: " << hr;
    SetModuleReleasedCallback({});
    return hr;
  }

  // Return the cookies on success.
  cookies = std::move(new_cookies);
  return hr;
}

// static
void Service::UnregisterClassObjects(base::HeapArray<DWORD>& cookies) {
  if (!cookies.empty()) {
    // Clear the callback registered with the process's WRL module.
    SetModuleReleasedCallback({});

    const HRESULT hr =
        Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule()
            .UnregisterCOMObject(nullptr, cookies.data(), cookies.size());
    if (FAILED(hr)) {
      LOG(ERROR) << "UnregisterCOMObject failed; hr: 0x" << std::hex << hr;
    }
    cookies = base::HeapArray<DWORD>();
  }
}

Service& Service::GetInstance() {
  return CHECK_DEREF(g_instance.load(std::memory_order_relaxed));
}

int Service::RunAsService() {
  static constexpr SERVICE_TABLE_ENTRY dispatch_table[] = {
      {const_cast<LPTSTR>(kWindowsServiceName), &Service::ServiceMainEntry},
      {nullptr, nullptr}};

  // This thread becomes the service control dispatcher thread for the process
  // upon the call to `::StartServiceCtrlDispatcher()`.

  // Upon this call, processing will continue on the service main thread in
  // `ServiceMainEntry()`. Processing will resume here when the service is
  // stopped. This same thread will process calls to `ServiceControlHandler()`.
  if (!::StartServiceCtrlDispatcher(dispatch_table)) {
    const auto error = ::GetLastError();

    // MSDN States: "If StartServiceCtrlDispatcher succeeds, it connects the
    // calling thread to the service control manager and does not return until
    // all running services in the process have entered the SERVICE_STOPPED
    // state." Despite that, https://crbug.com/380943791 is a case where a
    // service main thread is executing `ServiceMainEntry()` after the service
    // control dispatcher returns. Put the error code from a failure to start
    // the dispatcher into a crash key so that it is included in such crashes.
    static auto* const crash_key = base::debug::AllocateCrashKeyString(
        "Service-DispatcherError", base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(crash_key, base::NumberToString(error));

    PLOG(ERROR) << "Failed to connect to the service control manager";
    return error;
  }

  // In the case where the delegate does not implement `Run()`:
  // * Take the lock both for the sake of accessing service_status_ and to wait
  //   for the thread in `OnModuleReleased()` to complete the call.
  // In the case where the delegate implements `Run()`:
  // * Take the lock both for the sake of accessing service_status_ and to wait
  //   for the `ServiceMainImpl` thread to complete.
  base::AutoLock lock(lock_);
  return service_status_.dwWin32ExitCode;
}

void Service::StopService() {
  // This will cause the service control dispatcher to exit on the main thread.
  // Processing will continue in `RunAsService()`. After this call, the SCM will
  // launch a new process to handle inbound requests even if this process takes
  // time to clean up and terminate.
  SetServiceStatus(SERVICE_STOPPED);
}

void Service::ServiceMainImpl(const base::CommandLine& command_line) {
  base::AutoLock lock(lock_);

  service_status_handle_ = ::RegisterServiceCtrlHandler(
      kWindowsServiceName, &Service::ServiceControlHandler);
  if (service_status_handle_ == nullptr) {
    PLOG(ERROR) << "RegisterServiceCtrlHandler failed";
    return;
  }

  // Initialize this thread into the process's MTA.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  HRESULT hr = com_initializer.hr();
  if (SUCCEEDED(hr)) {
    // Tell the service control manager that the service is now running, and
    // will accept a stop request.
    SetServiceStatus(SERVICE_RUNNING);
    // Start the service.
    hr = Run(command_line);
  } else {
    PLOG(ERROR) << "Failed to initialize COM; hr = 0x" << std::hex << hr;
  }

  if (FAILED(hr)) {
    // Shut down immediately in case of error.
    service_status_.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
    service_status_.dwServiceSpecificExitCode = hr;
    SetServiceStatus(SERVICE_STOPPED);
  } else if (delegate_implements_run_) {
    // Shut down immediately if the delegate provided its own `Run()`.
    SetServiceStatus(SERVICE_STOPPED);
  }
}

int Service::RunInteractive() {
  base::WaitableEvent exit_event;
  base::AutoReset<raw_ptr<base::WaitableEvent>> reset_stop_event(
      &interactive_stop_event_, &exit_event);

  base::AutoLock lock(lock_);
  if (HRESULT hr = Run(*base::CommandLine::ForCurrentProcess());
      FAILED(hr) || delegate_implements_run_) {
    // Return immediately on error or if the service provided its own `Run()`.
    return hr;
  }

  {
    base::AutoUnlock unlock(lock_);
    // Wait for StopInteractive to be called.
    exit_event.Wait();
  }

  return S_OK;
}

void Service::StopInteractive() {
  CHECK_DEREF(interactive_stop_event_.get()).Signal();
}

// static
void Service::ServiceControlHandler(DWORD control) {
  if (control == SERVICE_CONTROL_STOP) {
    GetInstance().OnStopRequested();
  }
}

// static
void WINAPI Service::ServiceMainEntry(DWORD argc, wchar_t* argv[]) {
  if (Service* instance = g_instance.load(std::memory_order_relaxed)) {
    instance->ServiceMainImpl(base::CommandLine(argc, argv));
  } else {
    // There are cases where this function is called when there is no active
    // Service instance; see https://crbug.com/380943791.
    HandleSpuriousServiceMain(argc, argv);
  }
}

void Service::SetServiceStatus(DWORD state) {
  if (service_status_handle_) {
    service_status_.dwCurrentState = state;
    ::SetServiceStatus(service_status_handle_, &service_status_);
    if (state == SERVICE_STOPPED) {
      // The handle becomes invalid once STOPPED has been sent.
      service_status_handle_ = nullptr;
    }
  }
}

HRESULT Service::Run(const base::CommandLine& command_line) {
  if (HRESULT hr = InitializeComSecurity(); FAILED(hr)) {
    return hr;
  }

  // Enable fast rundown so that COM stubs are run down (i.e., have their
  // outstanding reference counts released) more quickly upon client
  // termination. Testing shows that "fast" rundown still takes on the order of
  // ten seconds, so it is not a substitute for the elevated tracing service's
  // ProcessWatcher.
  // https://learn.microsoft.com/nb-no/archive/blogs/distributedservices/com-server-cleanup-now-you-can-opt-for-a-fast-rundown
  if (Microsoft::WRL::ComPtr<IGlobalOptions> options; SUCCEEDED(
          ::CoCreateInstance(CLSID_GlobalOptions, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(&options)))) {
    options->Set(COMGLB_RO_SETTINGS, COMGLB_FAST_RUNDOWN);
  }

  // If `PreRun` returned `true`, the delegate's `Run` method is expected to do
  // all the logic of registering/unregistering classes and running the COM
  // server.
  if (delegate_implements_run_) {
    // The `lock_` (acquired at the beginning of `ServiceMainImpl`) is held
    // throughout the delegate's execution.
    // * `OnStopRequested()` will be called on the service control dispatcher
    //   thread if the service receives a stop request.
    // * the delegate should implement `OnServiceControlStop()` and stop itself
    //   (i.e., return from `delegate_->Run()`) when `OnStopRequested()` is
    //   called.
    return delegate_->Run(command_line);
  }

  // Registering class objects is sufficient for the service to be running.
  // Unretained is safe here because the callback is cleared in
  // `UnregisterClassObjects()`, which is run under lock during shutdown.
  return RegisterClassObjects(
      *delegate_,
      base::BindOnce(&Service::OnModuleReleased, base::Unretained(this)),
      cookies_);
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

void Service::OnModuleReleased() {
  CHECK(!delegate_implements_run_);

  base::AutoLock lock(lock_);

  // It is tempting to send a STOP_PENDING message to the service control
  // manager to tell it that shutdown has started. Unfortunately, it seems that
  // doing so does nothing to reduce errors on rapid reuse. It could be that
  // doing so actually increases the errors, as it delays shutdown.

  // Revoke the service's class objects.
  UnregisterClassObjects(cookies_);
  // Exit the service.
  (this->*exit_routine_)();
}

void Service::OnStopRequested() {
  // Tell the delegate that a stop has been requested. Do this before running
  // the exit routine, as that will cause the service control dispatcher to
  // return on the main thread.
  delegate_->OnServiceControlStop();
  if (delegate_implements_run_) {
    // `ServiceMainImpl()` notifies the SCM that the service has stopped when
    // the delegate's `Run()` returns.
    return;
  }

  base::AutoLock lock(lock_);

  // Revoke the service's class objects.
  UnregisterClassObjects(cookies_);
  // Exit the service.
  (this->*exit_routine_)();
}
