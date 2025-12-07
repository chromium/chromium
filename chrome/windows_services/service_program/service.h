// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_H_

#include <windows.h>

#include <wrl/implements.h>

#include "base/containers/heap_array.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace base {

class CommandLine;
class WaitableEvent;

}  // namespace base

class ServiceDelegate;

// The runner for a Windows service hosting a COM server.
class Service {
 public:
  explicit Service(ServiceDelegate& delegate);
  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;
  ~Service();

  // This function parses the command line and selects the action routine.
  bool InitWithCommandLine(const base::CommandLine* command_line);

  // Runs the service, returning the exit code reported to the service control
  // manager.
  int Start();

  // The following methods are public for the sake of testing.

  // Registers the Service COM class factory object(s) so other applications can
  // connect to it/them. Returns the registration status.
  static HRESULT RegisterClassObjects(ServiceDelegate& delegate,
                                      base::OnceClosure on_module_released,
                                      base::HeapArray<DWORD>& cookies);

  // Unregisters the Service COM class factory object(s).
  static void UnregisterClassObjects(base::HeapArray<DWORD>& cookies);

 private:
  static Service& GetInstance();

  // This function handshakes with the service control manager and starts
  // the service.
  int RunAsService();

  // Tells the service control manager that the service has stopped.
  void StopService() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Runs the service on the service thread. `command_line` may be different
  // from the command line with which the program was originally invoked.
  // Specifically, when the service is invoked as a COM server, the
  // `command_line` includes the `ServiceParameters` registered under the
  // `AppId` key.
  void ServiceMainImpl(const base::CommandLine& command_line);

  // Runs as a local server for testing purposes. RunInteractive returns an
  // HRESULT, not a Win32 error code.
  int RunInteractive();

  // Stops the service when running for testing purposes.
  void StopInteractive() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // The control handler of the service.
  static void WINAPI ServiceControlHandler(DWORD control);

  // The main service entry point.
  static void WINAPI ServiceMainEntry(DWORD argc, wchar_t* argv[]);

  // Calls ::SetServiceStatus().
  void SetServiceStatus(DWORD state) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Handles object registration. Returns a success result if the service is
  // operational.
  HRESULT Run(const base::CommandLine& command_line)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Calls ::CoInitializeSecurity to allow all users to create COM objects
  // within the server.
  static HRESULT InitializeComSecurity();

  // Revokes the service's class objects and stops the service. Called when the
  // last object is released.
  void OnModuleReleased();

  // Revokes the service's class objects and stops the service. Called when the
  // service control manager asks the service to stop.
  void OnStopRequested();

  const raw_ref<ServiceDelegate> delegate_;

  // The action routine to be executed.
  int (Service::*run_routine_)() = &Service::RunAsService;

  // The exit routine to be executed.
  void (Service::*exit_routine_)() = &Service::StopService;

  // An event waited on by `RunInteractive()` and signaled by
  // `StopInteractive()`.
  raw_ptr<base::WaitableEvent> interactive_stop_event_ = nullptr;

  // True if the delegate provides its own `Run()`.
  bool delegate_implements_run_ = false;

  // Serialize access to the members that are modified during shutdown, since
  // `OnModuleReleased()` will be called on an RPC thread handling module
  // release following a COM call.
  base::Lock lock_;

  // The service's status handle. Valid once the service control handler is
  // installed until the status is changed to SERVICE_STOPPED.
  SERVICE_STATUS_HANDLE service_status_handle_ GUARDED_BY(lock_) = nullptr;

  // The service's status reported to the service control manager via
  // SetServiceStatus.
  SERVICE_STATUS service_status_ GUARDED_BY(lock_){
      .dwServiceType = SERVICE_WIN32_OWN_PROCESS,
      .dwCurrentState = SERVICE_STOPPED,
      .dwControlsAccepted = SERVICE_ACCEPT_STOP};

  // Identifier of registered class objects used for revocation.
  base::HeapArray<DWORD> cookies_ GUARDED_BY(lock_);
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_H_
