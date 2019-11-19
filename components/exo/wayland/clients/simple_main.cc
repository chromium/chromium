// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/simple.h"

#include <limits>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "components/exo/wayland/clients/simple.h"

namespace switches {
// Specifies if VSync timing updates should be logged on the output.
const char kLogVSyncTimingUpdates[] = "log-vsync-timing-updates";
}  // namespace switches

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  exo::wayland::clients::ClientBase::InitParams params;
  if (!params.FromCommandLine(*command_line))
    return 1;

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::Simple client;
  if (!client.Init(params))
    return 1;

  bool log_vsync_timing_updates =
      command_line->HasSwitch(switches::kLogVSyncTimingUpdates);

  client.Run(std::numeric_limits<int>::max(), log_vsync_timing_updates);

  return 0;
}
