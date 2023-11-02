// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_REBOOTER_H_
#define CHROME_CHROME_CLEANER_OS_REBOOTER_H_

#include <string>

#include "base/command_line.h"
#include "chrome/chrome_cleaner/os/rebooter_api.h"

namespace chrome_cleaner {

// This class implements the |RebooterAPI| for production code.
class Rebooter : public RebooterAPI {
 public:
  static bool IsPostReboot();

  explicit Rebooter(const std::wstring& product_shortname);
  ~Rebooter() override {}

  // RebooterAPI implementation.
  void AppendPostRebootSwitch(const std::string& switch_string) override;
  void AppendPostRebootSwitchASCII(const std::string& switch_string,
                                   const std::string& value) override;
  bool RegisterPostRebootRun(const base::CommandLine* command_line,
                             const std::string& cleanup_id,
                             ExecutionMode execution_mode,
                             bool logs_uploads_enabled) override;
  void UnregisterPostRebootRun() override;

 private:
  std::wstring product_shortname_;
  base::CommandLine switches_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_REBOOTER_H_
