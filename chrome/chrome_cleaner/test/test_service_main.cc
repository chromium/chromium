// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_checker.h"
#include "chrome/chrome_cleaner/test/test_strings.h"

namespace {

class TestService {
 public:
  TestService() {
    service_status_.dwCurrentState = SERVICE_START_PENDING;
    service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    // action_max_timeout can be overridden by command line to a value that
    // could overflow.
    service_status_.dwWaitHint = base::checked_cast<DWORD>(
        TestTimeouts::action_max_timeout().InMilliseconds());
    service_stop_event_ = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
  }

  void SignalStopEvent() { ::SetEvent(service_stop_event_); }
  void WaitForStopEvent() {
    ::WaitForSingleObject(service_stop_event_, INFINITE);
  }

  void SetControlsAccepted(DWORD control) {
    DCHECK_CALLED_ON_VALID_THREAD(service_status_thread_checker_);
    service_status_.dwControlsAccepted = control;
  }

  void SetStatusHandle(SERVICE_STATUS_HANDLE status_handle) {
    CHECK(status_handle);
    status_handle_ = status_handle;
  }

  DWORD GetServiceStatusState() {
    DCHECK_CALLED_ON_VALID_THREAD(service_status_thread_checker_);
    return service_status_.dwCurrentState;
  }

  void SetServiceStatusState(DWORD state) {
    DCHECK_CALLED_ON_VALID_THREAD(service_status_thread_checker_);
    DCHECK(status_handle_);
    service_status_.dwCurrentState = state;
    if (::SetServiceStatus(status_handle_, &service_status_) == FALSE)
      LOG(ERROR) << "Cannot set service status state.";
  }

 private:
  SERVICE_STATUS_HANDLE status_handle_{};
  SERVICE_STATUS service_status_{};
  HANDLE service_stop_event_{INVALID_HANDLE_VALUE};
  THREAD_CHECKER(service_status_thread_checker_);
};

DWORD WINAPI ServiceCtrlHandler(DWORD control,
                                DWORD /* event_type */,
                                LPVOID /* event_data */,
                                LPVOID context) {
  TestService* service = reinterpret_cast<TestService*>(context);
  DCHECK(service);
  if (control == SERVICE_CONTROL_STOP)
    service->SignalStopEvent();
  return 0;
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
  TestService service;

  // Registers a function to handle extended service control requests.
  SERVICE_STATUS_HANDLE status_handle =
      RegisterServiceCtrlHandlerEx(L"", ServiceCtrlHandler, &service);
  service.SetStatusHandle(status_handle);

  // Tell the service controller the current service has started.
  service.SetServiceStatusState(SERVICE_START_PENDING);
  LOG(INFO) << "Service status is 'start pending'.";

  // Tell the service controller the current service is running.
  service.SetControlsAccepted(SERVICE_ACCEPT_STOP);
  service.SetServiceStatusState(SERVICE_RUNNING);
  LOG(INFO) << "Service status is 'running'.";

  // The body of the service wait until a stop signal is received.
  LOG(INFO) << "Service is waiting to be stopped.";
  service.WaitForStopEvent();

  // TODO(pmbureau): Add more kind of worker that may protect against service
  //     termination and validate service termination.

  // Tell the service controller the current service is stopped.
  service.SetServiceStatusState(SERVICE_STOP_PENDING);
  LOG(INFO) << "Service status is 'stop pending'.";

  service.SetServiceStatusState(SERVICE_STOPPED);
  // Do not attempt to perform any additional work after calling
  // SetServiceStatus with SERVICE_STOPPED, because the service process can be
  // terminated at any time.
}

constexpr base::char16 kLogFileExtension[] = L"log";

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  bool success = base::CommandLine::Init(0, nullptr);
  DCHECK(success);

  TestTimeouts::Initialize();

  // Initialize the logging settings to set a specific log file name.
  base::FilePath exe_file_path;
  success = base::PathService::Get(base::FILE_EXE, &exe_file_path);
  DCHECK(success);

  base::FilePath log_file_path(
      exe_file_path.ReplaceExtension(kLogFileExtension));
  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_FILE;
  logging_settings.log_file_path = log_file_path.value().c_str();
  success = logging::InitLogging(logging_settings);
  DCHECK(success);

  LOG(INFO) << "Service Started.";

  // Service main.

  // SERVICE_TABLE_ENTRY::lpServiceName takes a non-const string.
  base::char16 empty_string[] = L"";

  SERVICE_TABLE_ENTRY dispatch_table[] = {{empty_string, ServiceMain},
                                          {nullptr, nullptr}};

  if (::StartServiceCtrlDispatcher(dispatch_table) == FALSE) {
    LOG(ERROR) << "StartServiceCtrlDispatcher failed.";
    return 1;
  }

  LOG(INFO) << "Service ended.";
  return 0;
}
