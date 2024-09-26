// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_H_

#include <windows.h>

#include <wrl/implements.h>

#include "base/containers/heap_array.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"

namespace base {

class CommandLine;

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

  // Start() is the entry point called by WinMain.
  int Start();

  // The following methods are public for the sake of testing.

  // Registers the Service COM class factory object(s) so other applications can
  // connect to it/them. Returns the registration status.
  HRESULT RegisterClassObjects();

  // Unregisters the Service COM class factory object(s).
  void UnregisterClassObjects();

 private:
  static Service& GetInstance();

  // This function handshakes with the service control manager and starts
  // the service.
  int RunAsService();

  // Runs the service on the service thread.
  void ServiceMainImpl();

  // Runs as a local server for testing purposes. RunInteractive returns an
  // HRESULT, not a Win32 error code.
  int RunInteractive();

  // The control handler of the service.
  static void WINAPI ServiceControlHandler(DWORD control);

  // The main service entry point.
  static void WINAPI ServiceMainEntry(DWORD argc, wchar_t* argv[]);

  // Calls ::SetServiceStatus().
  void SetServiceStatus(DWORD state);

  // Handles object registration, message loop, and unregistration. Returns
  // when all registered objects are released.
  HRESULT Run();

  // Calls ::CoInitializeSecurity to allow all users to create COM objects
  // within the server.
  static HRESULT InitializeComSecurity();

  // Called when the last object is released or if the service is asked to exit.
  void SignalExit();

  // Registers `factory` as the factory for the service identified by `id`.
  void RegisterClassFactory(const std::u16string& id, IClassFactory* factory);

  const raw_ref<ServiceDelegate> delegate_;

  // The action routine to be executed.
  int (Service::*run_routine_)() = &Service::RunAsService;

  SERVICE_STATUS_HANDLE service_status_handle_ = nullptr;
  SERVICE_STATUS service_status_{.dwServiceType = SERVICE_WIN32_OWN_PROCESS,
                                 .dwCurrentState = SERVICE_STOPPED,
                                 .dwControlsAccepted = SERVICE_ACCEPT_STOP};

  // Identifier of registered class objects used for unregistration.
  base::HeapArray<DWORD> cookies_;

  // A closure that is run when the last COM instance is released, or if the
  // service control manager asks the service to exit.
  base::RepeatingClosure quit_closure_;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_SERVICE_H_
