// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_REBOOTER_API_H_
#define CHROME_CHROME_CLEANER_OS_REBOOTER_API_H_

#include <string>

#include "base/command_line.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

// This class is used as a wrapper around the OS calls related to reboot.
class RebooterAPI {
 public:
  virtual ~RebooterAPI() {}

  // Add command line switches to be added to the post reboot run.
  virtual void AppendPostRebootSwitch(const std::string& switch_string) = 0;
  virtual void AppendPostRebootSwitchASCII(const std::string& switch_string,
                                           const std::string& value) = 0;

  // Register the currently running executable to run once after the reboot
  // with the post reboot command line switch. Return false on failure.
  // |cleanup_id| will be added to the post reboot command line.
  // |execution_mode| should be the currently running executable's execution
  // mode.
  virtual bool RegisterPostRebootRun(
      const base::CommandLine* current_command_line,
      const std::string& cleanup_id,
      ExecutionMode execution_mode,
      bool logs_uploads_allowed) = 0;

  // Remove the entries to run Chrome Cleanup after reboot.
  virtual void UnregisterPostRebootRun() = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_REBOOTER_API_H_
