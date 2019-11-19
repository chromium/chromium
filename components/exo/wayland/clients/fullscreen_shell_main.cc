// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/fullscreen_shell.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  exo::wayland::clients::FullscreenClient::InitParams params;

  params.allocate_buffers_with_output_mode = true;
  params.use_fullscreen_shell = true;
  params.use_touch = true;

  if (!params.FromCommandLine(*command_line))
    return 1;

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  exo::wayland::clients::FullscreenClient client;

  if (!client.Init(params))
    return 1;

  if (!client.Run(params))
    return 1;

  return 0;
}
