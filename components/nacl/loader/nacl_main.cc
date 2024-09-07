// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/message_loop/message_pump_type.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/task/single_thread_task_executor.h"
#include "base/timer/hi_res_timer_manager.h"
#include "build/build_config.h"
#include "components/nacl/loader/nacl_listener.h"
#include "components/nacl/loader/nacl_main_platform_delegate.h"
#include "components/power_monitor/make_power_monitor_device_source.h"
#include "content/public/common/main_function_params.h"
#include "mojo/core/embedder/embedder.h"
#include "sandbox/policy/switches.h"

// main() routine for the NaCl loader process.
int NaClMain(content::MainFunctionParams parameters) {
  const base::CommandLine& parsed_command_line = *parameters.command_line;

  // The Mojo EDK must be initialized before using IPC.
  mojo::core::InitFeatures();
  mojo::core::Init();

  // The main thread of the plugin services IO.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::IO);
  base::PlatformThread::SetName("CrNaClMain");

  base::PowerMonitor::GetInstance()->Initialize(MakePowerMonitorDeviceSource());
  base::HighResolutionTimerManager hi_res_timer_manager;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  NaClMainPlatformDelegate platform;
  bool no_sandbox =
      parsed_command_line.HasSwitch(sandbox::policy::switches::kNoSandbox);

#if BUILDFLAG(IS_POSIX)
  // The number of cores must be obtained before the invocation of
  // platform.EnableSandbox(), so cannot simply be inlined below.
  int number_of_cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif

  if (!no_sandbox) {
    platform.EnableSandbox(parameters);
  }
  NaClListener listener;
#if BUILDFLAG(IS_POSIX)
  listener.set_number_of_cores(number_of_cores);
#endif

  listener.Listen();
#else
  NOTIMPLEMENTED() << " not implemented startup, plugin startup dialog etc.";
#endif
  return 0;
}
