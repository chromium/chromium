// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/test/base/chromeos/crosier/helper/switches.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"

namespace {

void Usage() {
  printf("Usage:\n");
  printf("  %s --%s=<server socket path> -- [remote command]\n",
         base::CommandLine::ForCurrentProcess()
             ->GetProgram()
             .BaseName()
             .value()
             .c_str(),
         crosier::kSwitchSocketPath);
}

bool RunCommand(const std::string_view command) {
  LOG(INFO) << "Run: " << command;
  return TestSudoHelperClient().RunCommand(command).return_code == 0;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);

  logging::InitLogging(logging::LoggingSettings());

  base::CommandLine* command = base::CommandLine::ForCurrentProcess();
  if (!command->HasSwitch(crosier::kSwitchSocketPath)) {
    Usage();
    return 0;
  }

  auto args = command->GetArgs();
  if (args.empty()) {
    Usage();
    return 0;
  }

  return RunCommand(base::JoinString(args, " ")) ? 0 : -1;
}
