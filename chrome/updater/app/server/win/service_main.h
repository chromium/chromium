// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_WIN_SERVICE_MAIN_H_
#define CHROME_UPDATER_APP_SERVER_WIN_SERVICE_MAIN_H_

#include <windows.h>

#include "base/no_destructor.h"
#include "base/win/atl.h"

namespace base {

class CommandLine;

}  // namespace base

namespace updater {

class ServiceMain {
 public:
  // This function is the main entry point for the service. The return value can
  // be either a Win32 error code or an HRESULT, depending on the API function
  // that failed.
  static int RunWindowsService(const base::CommandLine* command_line);

  static ServiceMain* GetInstance();

  // This function parses the command line and selects between running as a
  // Service or running interactively if "--console".
  bool InitWithCommandLine(const base::CommandLine* command_line);

  // Start() is the entry point called by WinMain.
  int Start();

  ServiceMain(const ServiceMain&) = delete;
  ServiceMain& operator=(const ServiceMain&) = delete;

 private:
  ServiceMain();
  ~ServiceMain();

  // This function handshakes with the service control manager and starts
  // the service.
  int RunAsService();

  // Runs the service on the service thread.
  void ServiceMainImpl(const base::CommandLine& command_line);

  // Runs as a local server for testing purposes. RunInteractive returns an
  // HRESULT, not a Win32 error code.
  int RunInteractive();

  // The control handler of the service.
  static void WINAPI ServiceControlHandler(DWORD control);

  // The main service entry point.
  static void WINAPI ServiceMainEntry(DWORD argc, wchar_t* argv[]);

  // Calls ::SetServiceStatus().
  void SetServiceStatus(DWORD state);

  // Runs the main logic of the service.
  HRESULT Run(const base::CommandLine& command_line);

  // Handles COM object registration, message loop, and unregistration. Returns
  // when all COM objects are released.
  HRESULT RunCOMServer();

  // Calls ::CoInitializeSecurity to allow all users to create COM objects
  // within the server.
  static HRESULT InitializeComSecurity();

  // The action routine to be executed.
  int (ServiceMain::*run_routine_)() = &ServiceMain::RunAsService;

  SERVICE_STATUS_HANDLE service_status_handle_ = nullptr;
  SERVICE_STATUS service_status_ = {};

  friend class base::NoDestructor<ServiceMain>;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_WIN_SERVICE_MAIN_H_
