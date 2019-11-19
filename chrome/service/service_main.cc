// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/debug/debugger.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "build/build_config.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/service_process.h"
#include "content/public/common/main_function_params.h"
#include "net/url_request/url_request.h"

// Mainline routine for running as the Cloud Print service process.
int CloudPrintServiceProcessMain(
    const content::MainFunctionParams& parameters) {
  // Chrome disallows cookies by default. All code paths that want to use
  // cookies should go through the browser process.
  net::URLRequest::SetDefaultCookiePolicyToBlock();

  base::PlatformThread::SetName("CrServiceMain");

#if defined(OS_WIN)
  // The service process needs to be able to process WM_QUIT messages from the
  // Cloud Print Service UI on Windows.
  base::SingleThreadTaskExecutor main_task_executor(
      base::MessagePumpType::UI_WITH_WM_QUIT_SUPPORT);
#else
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
#endif

  if (parameters.command_line.HasSwitch(switches::kWaitForDebugger)) {
    base::debug::WaitForDebugger(60, true);
  }

  VLOG(1) << "Service process launched: "
          << parameters.command_line.GetCommandLineString();

  // If there is already a service process running, quit now.
  std::unique_ptr<ServiceProcessState> state(new ServiceProcessState);
  if (!state->Initialize())
    return 0;

  base::RunLoop run_loop;
  ServiceProcess service_process;
  if (service_process.Initialize(run_loop.QuitClosure(),
                                 parameters.command_line, std::move(state))) {
    run_loop.Run();
  } else {
    LOG(ERROR) << "Service process failed to initialize";
  }
  return 0;
}
