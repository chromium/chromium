// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/profiler/chrome_thread_profiler_client.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/utility/services.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/sampling_profiler/process_type.h"
#include "components/sampling_profiler/thread_profiler.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#endif

ChromeContentUtilityClient::ChromeContentUtilityClient() {
  sampling_profiler::ThreadProfiler::SetClient(
      std::make_unique<ChromeThreadProfilerClient>());
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() = default;

void ChromeContentUtilityClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
#if BUILDFLAG(IS_WIN)
  auto& cmd_line = *base::CommandLine::ForCurrentProcess();
  auto sandbox_type = sandbox::policy::SandboxTypeFromCommandLine(cmd_line);
  utility_process_running_elevated_ =
      sandbox_type == sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges;
#endif
}

void ChromeContentUtilityClient::UtilityThreadStarted() {
  // Only builds message pipes for utility processes which enable sampling
  // profilers.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  // An in-process utility thread may run in other processes, only set up
  // collector in a utility process.
  if (process_type == switches::kUtilityProcess) {
    const auto* heap_profiler_controller =
        heap_profiling::HeapProfilerController::GetInstance();
    // The HeapProfilerController should have been created in
    // ChromeMainDelegate::PostEarlyInitialization.
    CHECK(heap_profiler_controller);
    if (ThreadProfilerConfiguration::Get()
            ->IsProfilerEnabledForCurrentProcess() ||
        heap_profiler_controller->IsEnabled()) {
      mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector;
      content::ChildThread::Get()->BindHostReceiver(
          collector.InitWithNewPipeAndPassReceiver());
      metrics::CallStackProfileBuilder::
          SetParentProfileCollectorForChildProcess(std::move(collector));
    }
  }
}

void ChromeContentUtilityClient::RegisterMainThreadServices(
    mojo::ServiceFactory& services) {
  if (utility_process_running_elevated_)
    return ::RegisterElevatedMainThreadServices(services);
  return ::RegisterMainThreadServices(services);
}

void ChromeContentUtilityClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner* io_thread_task_runner) {
  io_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&sampling_profiler::ThreadProfiler::StartOnChildThread,
                     sampling_profiler::ProfilerThreadType::kIo));
}

void ChromeContentUtilityClient::RegisterIOThreadServices(
    mojo::ServiceFactory& services) {
  return ::RegisterIOThreadServices(services);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
mojo::GenericPendingReceiver
ChromeContentUtilityClient::InitMojoServiceManager() {
  return ash::mojo_service_manager::BootstrapServiceManagerInUtilityProcess();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
