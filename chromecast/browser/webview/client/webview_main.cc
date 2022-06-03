// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/client/webview.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Usage: wayland_webview_client [CHANNEL DIRECTORY] [OPTION]"
              << std::endl;
    return -1;
  }

  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  chromecast::client::WebviewClient::InitParams params;
  params.allocate_buffers_with_output_mode = true;
  params.num_buffers = 3;
  params.use_fullscreen_shell = true;
  params.use_touch = true;

  if (!params.FromCommandLine(*command_line))
    return 1;

  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::IO);
  chromecast::client::WebviewClient client;
  client.Init(params);
  client.Run(params, argv[1]);
}
