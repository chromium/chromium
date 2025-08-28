// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/test_support/service_environment.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/path_service.h"
#include "chrome/common/env_vars.h"
#include "chrome/windows_services/service_program/switches.h"

namespace {

// Adds the --unattended-test switch to the service's command line if the test
// is running with CHROME_HEADLESS in its environment block.
void AddUnattendedTestSwitch(base::CommandLine& command_line) {
  if (auto env = base::Environment::Create();
      env->HasVar(env_vars::kHeadless)) {
    command_line.AppendSwitch(switches::kUnattendedTest);
  }
}

}  // namespace

ServiceEnvironment::ServiceEnvironment(
    std::wstring_view display_name,
    base::FilePath::StringViewType service_exe_name,
    std::string_view testing_switch,
    const CLSID& clsid,
    const IID& iid) {
  std::wstring service_name(display_name);
  std::erase(service_name, L' ');

  base::CommandLine service_command(
      base::PathService::CheckedGet(base::DIR_EXE).Append(service_exe_name));

  AddUnattendedTestSwitch(service_command);

  if (!testing_switch.empty()) {
    service_command.AppendSwitch(testing_switch);
  }
  log_grabber_.AddLoggingSwitches(service_command);

  service_.emplace(service_name, display_name, /*description=*/display_name,
                   std::move(service_command), clsid, iid);
  if (!service_->is_valid()) {
    service_.reset();
  }
}

ServiceEnvironment::~ServiceEnvironment() = default;

base::Process ServiceEnvironment::GetRunningService() {
  return is_valid() && service_->is_valid() ? service_->GetRunningService()
                                            : base::Process();
}

void ServiceEnvironment::SetLogMessageCallback(
    ScopedLogGrabber::LogMessageCallback callback) {
  log_grabber_.SetLogMessageCallback(std::move(callback));
}
