// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/debug/debugger.h"
#include "base/memory/shared_memory_hooks.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "build/build_config.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/service_process.h"
#include "content/public/common/main_function_params.h"
#include "net/url_request/url_request.h"

// Mainline routine for running as the Cloud Print service process.
int CloudPrintServiceProcessMain(content::MainFunctionParams parameters) {
  // This is a hack: the Cloud Print service doesn't actually set up a sandbox,
  // but sandbox::policy::SandboxTypeFromCommandLine(command_line)) doesn't know
  // about it, so it's considered sandboxed, causing shared memory hooks to be
  // installed above. The Cloud Print service *also* doesn't set
  // is_broker_process when initializing Mojo, so that bit also can't be used to
  // determine whether or not to install the shared memory hooks.
  //
  // Since the Cloud Print service is supposed to go away at some point soon,
  // just remove the hooks here.
  base::SharedMemoryHooks::SetCreateHooks(nullptr, nullptr, nullptr);

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

  if (parameters.command_line->HasSwitch(switches::kWaitForDebugger)) {
    base::debug::WaitForDebugger(60, true);
  }

  VLOG(1) << "Service process launched: "
          << parameters.command_line->GetCommandLineString();

  auto initialize_service = [](ServiceProcessState* service) {
    for (int i = 0; i < 10; ++i) {
      if (service->Initialize())
        return true;
      base::PlatformThread::Sleep(base::Milliseconds(i * 100));
    }
    return false;
  };

  // If there is already a service process running, quit now. Retry a few times
  // in case the running service is busy exiting.
  // TODO(ellyjones): Are these retries actually necessary / can this case
  // happen in practice?
  auto state = std::make_unique<ServiceProcessState>();
  if (!initialize_service(state.get()))
    return 0;

  base::RunLoop run_loop;
  ServiceProcess service_process;
  if (service_process.Initialize(run_loop.QuitClosure(),
                                 *parameters.command_line, std::move(state))) {
    run_loop.Run();
  } else {
    LOG(ERROR) << "Service process failed to initialize";
  }
  return 0;
}
