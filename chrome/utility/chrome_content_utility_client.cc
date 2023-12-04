// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include <stddef.h>

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/profiler/thread_profiler.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/utility/browser_exposed_utility_interfaces.h"
#include "chrome/utility/services.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"

ChromeContentUtilityClient::ChromeContentUtilityClient() = default;

ChromeContentUtilityClient::~ChromeContentUtilityClient() = default;

void ChromeContentUtilityClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
#if BUILDFLAG(IS_WIN)
  auto& cmd_line = *base::CommandLine::ForCurrentProcess();
  auto sandbox_type = sandbox::policy::SandboxTypeFromCommandLine(cmd_line);
  utility_process_running_elevated_ =
      sandbox_type == sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges;
#endif

  // If our process runs with elevated privileges, only add elevated Mojo
  // interfaces to the BinderMap.
  //
  // NOTE: Do not add interfaces directly from within this method. Instead,
  // modify the definition of |ExposeElevatedChromeUtilityInterfacesToBrowser()|
  // to ensure security review coverage.
  if (!utility_process_running_elevated_)
    ExposeElevatedChromeUtilityInterfacesToBrowser(binders);
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
    // The HeapProfilerController should have been created in
    // ChromeMainDelegate::PostEarlyInitialization.
    using HeapProfilerController = heap_profiling::HeapProfilerController;
    DCHECK_NE(HeapProfilerController::GetProfilingEnabled(),
              HeapProfilerController::ProfilingEnabled::kNoController);
    if (ThreadProfiler::ShouldCollectProfilesForChildProcess() ||
        HeapProfilerController::GetProfilingEnabled() ==
            HeapProfilerController::ProfilingEnabled::kEnabled) {
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
      FROM_HERE, base::BindOnce(&ThreadProfiler::StartOnChildThread,
                                metrics::CallStackProfileParams::Thread::kIo));
}

void ChromeContentUtilityClient::RegisterIOThreadServices(
    mojo::ServiceFactory& services) {
  return ::RegisterIOThreadServices(services);
}
