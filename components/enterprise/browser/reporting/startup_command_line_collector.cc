// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/startup_command_line_collector.h"

#include "base/command_line.h"
#include "base/no_destructor.h"

namespace enterprise_reporting {

// static
StartupCommandLineCollector* StartupCommandLineCollector::GetInstance() {
  static base::NoDestructor<StartupCommandLineCollector> instance;
  return instance.get();
}

void StartupCommandLineCollector::CollectCommandLine() {
  CHECK(!startup_command_line_.has_value());
  startup_command_line_ = *base::CommandLine::ForCurrentProcess();
}

std::vector<std::string> StartupCommandLineCollector::GetCollectedSwitchKeys() const {
  std::vector<std::string> keys;
  if (startup_command_line_.has_value()) {
    for (const auto& it : startup_command_line_->GetSwitches()) {
      keys.push_back(it.first);
    }
  }
  return keys;
}

void StartupCommandLineCollector::ResetForTesting() {
  startup_command_line_.reset();
}

StartupCommandLineCollector::StartupCommandLineCollector() = default;

StartupCommandLineCollector::~StartupCommandLineCollector() = default;

}  // namespace enterprise_reporting
