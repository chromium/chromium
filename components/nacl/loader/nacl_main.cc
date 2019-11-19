// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/task/single_thread_task_executor.h"
#include "base/timer/hi_res_timer_manager.h"
#include "build/build_config.h"
#include "components/nacl/loader/nacl_listener.h"
#include "components/nacl/loader/nacl_main_platform_delegate.h"
#include "content/public/common/main_function_params.h"
#include "mojo/core/embedder/embedder.h"
#include "services/service_manager/sandbox/switches.h"

// main() routine for the NaCl loader process.
int NaClMain(const content::MainFunctionParams& parameters) {
  const base::CommandLine& parsed_command_line = parameters.command_line;

  // The Mojo EDK must be initialized before using IPC.
  mojo::core::Init();

  // The main thread of the plugin services IO.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::IO);
  base::PlatformThread::SetName("CrNaClMain");

  base::PowerMonitor::Initialize(
      std::make_unique<base::PowerMonitorDeviceSource>());
  base::HighResolutionTimerManager hi_res_timer_manager;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID)
  NaClMainPlatformDelegate platform;
  bool no_sandbox =
      parsed_command_line.HasSwitch(service_manager::switches::kNoSandbox);

#if defined(OS_POSIX)
  // The number of cores must be obtained before the invocation of
  // platform.EnableSandbox(), so cannot simply be inlined below.
  int number_of_cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif

  if (!no_sandbox) {
    platform.EnableSandbox(parameters);
  }
  NaClListener listener;
#if defined(OS_POSIX)
  listener.set_number_of_cores(number_of_cores);
#endif

  listener.Listen();
#else
  NOTIMPLEMENTED() << " not implemented startup, plugin startup dialog etc.";
#endif
  return 0;
}
