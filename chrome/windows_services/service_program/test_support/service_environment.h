// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SERVICE_ENVIRONMENT_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SERVICE_ENVIRONMENT_H_

#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/win/windows_types.h"
#include "chrome/windows_services/service_program/test_support/scoped_install_service.h"
#include "chrome/windows_services/service_program/test_support/scoped_log_grabber.h"

// Installs a test service for its lifetime.
class ServiceEnvironment {
 public:
  // Installs the service named `display_name`; stripping spaces from this to
  // make its actual service name. `service_exe_name` is the basename of the
  // executable hosting the service. It must be in the same directory as the
  // executable under test (e.g., the build output directory). `testing_switch`,
  // if not empty, will be added to the service's command line. `clsid` and
  // `iid` identify the COM class and interface exposed by the service.
  ServiceEnvironment(std::wstring_view display_name,
                     base::FilePath::StringViewType service_exe_name,
                     std::string_view testing_switch,
                     const CLSID& clsid,
                     const IID& iid);
  ServiceEnvironment(const ServiceEnvironment&) = delete;
  ServiceEnvironment& operator=(const ServiceEnvironment&) = delete;
  ~ServiceEnvironment();

  bool is_valid() const { return service_.has_value(); }

  // Sets a callback to be run for each message received from a service process.
  // The callback is run with the process ID of the service and the log message.
  // The message is not emitted to the test process's stderr if the callback
  // returns true.
  using LogMessageCallback = ScopedLogGrabber::LogMessageCallback;
  void SetLogMessageCallback(ScopedLogGrabber::LogMessageCallback callback);

 private:
  ScopedLogGrabber log_grabber_;
  std::optional<ScopedInstallService> service_;
};

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_TEST_SUPPORT_SERVICE_ENVIRONMENT_H_
