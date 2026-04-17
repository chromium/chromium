// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_STARTUP_COMMAND_LINE_COLLECTOR_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_STARTUP_COMMAND_LINE_COLLECTOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/no_destructor.h"

namespace enterprise_reporting {

// A singleton utility class that captures and provides access to the command
// line switches used during the initial launch of the browser. This information
// is collected early in the startup process for subsequent enterprise reporting.
class StartupCommandLineCollector {
 public:
  StartupCommandLineCollector(const StartupCommandLineCollector&) = delete;
  StartupCommandLineCollector& operator=(const StartupCommandLineCollector&) = delete;

  // Returns the singleton instance of this class.
  static StartupCommandLineCollector* GetInstance();

  // Collects the current command line and stores it.
  // This should be called exactly once early in the browser process startup.
  void CollectCommandLine();

  // Returns the collected switch keys.
  // Only switch keys are exposed; values are ignored for privacy.
  std::vector<std::string> GetCollectedSwitchKeys() const;

  // Resets the collected switches. Only for testing.
  void ResetForTesting();

 private:
  friend class base::NoDestructor<StartupCommandLineCollector>;

  StartupCommandLineCollector();
  ~StartupCommandLineCollector();

  std::optional<base::CommandLine> startup_command_line_;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_STARTUP_COMMAND_LINE_COLLECTOR_H_
