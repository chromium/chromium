// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/service_main.h"

#include <atlsecurity.h>
#include <sddl.h>

#include <ios>
#include <string>
#include <type_traits>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/task/single_thread_task_executor.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/app/app_server_win.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/win_util.h"

namespace updater {

namespace {

// Command line switch "--console" runs the service interactively for
// debugging purposes.
constexpr char kConsoleSwitchName[] = "console";

HRESULT RunWakeTask() {
  base::CommandLine run_updater_wake_command(
      base::CommandLine::ForCurrentProcess()->GetProgram());
  run_updater_wake_command.AppendSwitch(kWakeSwitch);
  run_updater_wake_command.AppendSwitch(kSystemSwitch);
  VLOG(2) << "Launching Wake command: "
          << run_updater_wake_command.GetCommandLineString();

  base::LaunchOptions options;
  options.start_hidden = true;
  const base::Process process =
      base::LaunchProcess(run_updater_wake_command, options);
  return process.IsValid() ? S_OK : HRESULTFromLastError();
}

}  // namespace

int ServiceMain::RunWindowsService(const base::CommandLine* command_line) {
  ServiceMain* service = ServiceMain::GetInstance();
  if (!service->InitWithCommandLine(command_line)) {
    return ERROR_BAD_ARGUMENTS;
  }

  int ret = service->Start();
  CHECK_NE(ret, int{STILL_ACTIVE});
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
  if (command_line->HasSwitch(kConsoleSwitchName)) {
    run_routine_ = &ServiceMain::RunInteractive;
  }

  return true;
}

// Start() is the entry point called by WinMain.
int ServiceMain::Start() {
  return (this->*run_routine_)();
}

ServiceMain::ServiceMain() {
  service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status_.dwCurrentState = SERVICE_STOPPED;
  service_status_.dwControlsAccepted = SERVICE_ACCEPT_STOP;
}

ServiceMain::~ServiceMain() {
  NOTREACHED_IN_MIGRATION();  // The instance of this class is a leaky
                              // singleton.
}

int ServiceMain::RunAsService() {
  const std::wstring service_name = GetServiceName(IsInternalService());
  const SERVICE_TABLE_ENTRY dispatch_table[] = {
      {const_cast<LPTSTR>(service_name.c_str()),
       &ServiceMain::ServiceMainEntry},
      {nullptr, nullptr}};

  if (!::StartServiceCtrlDispatcher(dispatch_table)) {
    service_status_.dwWin32ExitCode = ::GetLastError();
    PLOG(ERROR) << "Failed to connect to the service control manager";
  }

  return service_status_.dwWin32ExitCode;
}

void ServiceMain::ServiceMainImpl(const base::CommandLine& command_line) {
  service_status_handle_ =
      ::RegisterServiceCtrlHandler(GetServiceName(IsInternalService()).c_str(),
                                   &ServiceMain::ServiceControlHandler);
  if (service_status_handle_ == nullptr) {
    PLOG(ERROR) << "RegisterServiceCtrlHandler failed";
    return;
  }
  SetServiceStatus(SERVICE_RUNNING);

  // When the Run function returns, the service has stopped.
  // `hr` can be either an HRESULT or a Windows error code.
  const HRESULT hr = Run(command_line);
  if (hr != S_OK) {
    VLOG(2) << "Run returned: " << std::hex << hr;
    service_status_.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
    service_status_.dwServiceSpecificExitCode = hr;
  }

  SetServiceStatus(SERVICE_STOPPED);
}

int ServiceMain::RunInteractive() {
  return RunCOMServer();
}

// static
void ServiceMain::ServiceControlHandler(DWORD control) {
  ServiceMain* self = ServiceMain::GetInstance();
  switch (control) {
    case SERVICE_CONTROL_STOP:
      self->SetServiceStatus(SERVICE_STOP_PENDING);
      GetAppServerWinInstance()->Stop();
      break;

    default:
      break;
  }
}

// static
void WINAPI ServiceMain::ServiceMainEntry(DWORD argc, wchar_t* argv[]) {
  ServiceMain::GetInstance()->ServiceMainImpl(base::CommandLine(argc, argv));
}

void ServiceMain::SetServiceStatus(DWORD state) {
  service_status_.dwCurrentState = state;
  ::SetServiceStatus(service_status_handle_, &service_status_);
}

HRESULT ServiceMain::Run(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(kComServiceSwitch)) {
    VLOG(2) << "Running COM server within the Windows Service";
    return RunCOMServer();
  }

  if (IsInternalService()) {
    VLOG(2) << "Running Wake task from the Windows Service";
    return RunWakeTask();
  }

  return S_OK;
}

HRESULT ServiceMain::RunCOMServer() {
  base::SingleThreadTaskExecutor service_task_executor(
      base::MessagePumpType::DEFAULT);
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    LOG(ERROR) << "Failed to initialize COM";
    return CO_E_INITIALIZATIONFAILED;
  }
  HRESULT hr = InitializeComSecurity();
  if (FAILED(hr)) {
    return hr;
  }
  return GetAppServerWinInstance()->Run();
}

// static
HRESULT ServiceMain::InitializeComSecurity() {
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

}  // namespace updater
