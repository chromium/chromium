// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/blur.h"

#include <limits>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/blur.h"

namespace switches {

// Specifies the sigma X to use.
const char kSigmaX[] = "sigma-x";

// Specifies the sigma Y to use.
const char kSigmaY[] = "sigma-y";

// Max sigma to allow without adjusting scale factor.
const char kMaxSigma[] = "max-sigma";

}  // namespace switches

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  exo::wayland::clients::ClientBase::InitParams params;
  if (!params.FromCommandLine(*command_line))
    return 1;

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::Blur client;
  if (!client.Init(params))
    return 1;

  double sigma_x = 30.0;
  if (command_line->HasSwitch(switches::kSigmaX) &&
      !base::StringToDouble(
          command_line->GetSwitchValueASCII(switches::kSigmaX), &sigma_x)) {
    LOG(ERROR) << "Invalid value for " << switches::kSigmaX;
    return 1;
  }

  double sigma_y = 30.0;
  if (command_line->HasSwitch(switches::kSigmaY) &&
      !base::StringToDouble(
          command_line->GetSwitchValueASCII(switches::kSigmaY), &sigma_y)) {
    LOG(ERROR) << "Invalid value for " << switches::kSigmaY;
    return 1;
  }

  double max_sigma = 4.0;
  if (command_line->HasSwitch(switches::kMaxSigma) &&
      !base::StringToDouble(
          command_line->GetSwitchValueASCII(switches::kMaxSigma), &max_sigma)) {
    LOG(ERROR) << "Invalid value for " << switches::kMaxSigma;
    return 1;
  }

  client.Run(sigma_x, sigma_y, max_sigma, false /* offscreen */,
             std::numeric_limits<int>::max());

  return 0;
}
