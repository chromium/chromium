// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_SERVICE_MAIN_H_
#define CHROME_ELEVATION_SERVICE_SERVICE_MAIN_H_

#include <windows.h>
#include <wrl/implements.h>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"

namespace base {

class CommandLine;

}  // namespace base

namespace elevation_service {

class ServiceMain {
 public:
  static ServiceMain* GetInstance();

  // This function parses the command line and selects the action routine.
  bool InitWithCommandLine(const base::CommandLine* command_line);

  // Start() is the entry point called by WinMain.
  int Start();

  // The following methods are public for the sake of testing.

  // Creates an out-of-proc WRL Module.
  void CreateWRLModule();

  // Registers the Service COM class factory object so other applications can
  // connect to it. Returns the registration status.
  HRESULT RegisterClassObject();

  // Unregisters the Service COM class factory object.
  void UnregisterClassObject();

  // Returns true when the last COM object is released, or if the service is
  // asked to exit.
  bool IsExitSignaled();

  // Resets the state of the |exit_signal_| event.
  void ResetExitSignaled();

 private:
  ServiceMain();
  ~ServiceMain();

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
  static void WINAPI ServiceMainEntry(DWORD argc, base::char16* argv[]);

  // Calls ::SetServiceStatus().
  void SetServiceStatus(DWORD state);

  // Handles object registration, message loop, and unregistration. Returns
  // when all registered objects are released.
  HRESULT Run();

  // Calls ::CoInitializeSecurity to allow all users to create COM objects
  // within the server.
  static HRESULT InitializeComSecurity();

  // Waits until the last object is released or until the service is asked to
  // exit.
  void WaitForExitSignal();

  // Called when the last object is released or if the service is asked to exit.
  void SignalExit();

  // Registers |factory| as the factory for the elevator identified by |id|.
  void RegisterElevatorFactory(const base::string16& id,
                               IClassFactory* factory);

  // The action routine to be executed.
  int (ServiceMain::*run_routine_)();

  SERVICE_STATUS_HANDLE service_status_handle_;
  SERVICE_STATUS service_status_;

  // Identifier of registered class objects used for unregistration.
  DWORD cookies_[1];

  // This event is signaled when the last COM instance is released, or if the
  // service control manager asks the service to exit.
  base::WaitableEvent exit_signal_;

  friend class base::NoDestructor<ServiceMain>;

  DISALLOW_COPY_AND_ASSIGN(ServiceMain);
};

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_SERVICE_MAIN_H_
