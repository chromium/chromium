// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/multi_process/supervisor.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/heap_profiling/multi_process/client_connection_manager.h"
#include "components/services/heap_profiling/heap_profiling_service.h"
#include "components/services/heap_profiling/public/cpp/controller.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_coordinator_service.h"
#include "content/public/browser/tracing_controller.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace heap_profiling {

namespace {

base::trace_event::TraceConfig GetBackgroundTracingConfig(bool anonymize) {
  // Disable all categories other than memory-infra.
  base::trace_event::TraceConfig trace_config(
      "-*,disabled-by-default-memory-infra",
      base::trace_event::RECORD_UNTIL_FULL);

  // This flag is set by background tracing to filter out undesired events.
  if (anonymize)
    trace_config.EnableArgumentFilter();

  return trace_config;
}

}  // namespace

// static
Supervisor* Supervisor::GetInstance() {
  static base::NoDestructor<Supervisor> supervisor;
  return supervisor.get();
}

Supervisor::Supervisor() = default;
Supervisor::~Supervisor() {
  NOTREACHED_IN_MIGRATION();
}

bool Supervisor::HasStarted() {
  return started_;
}

void Supervisor::SetClientConnectionManagerConstructor(
    ClientConnectionManagerConstructor constructor) {
  DCHECK(!HasStarted());
  constructor_ = constructor;
}

void Supervisor::Start(base::OnceClosure closure) {
  Start(GetModeForStartup(), GetStackModeForStartup(),
        GetSamplingRateForStartup(), std::move(closure));
}

void Supervisor::Start(Mode mode,
                       mojom::StackMode stack_mode,
                       uint32_t sampling_rate,
                       base::OnceClosure closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!started_);
  base::trace_event::MemoryDumpManager::GetInstance()
      ->GetDumpThreadTaskRunner()
      ->PostTask(FROM_HERE,
                 base::BindOnce(&Supervisor::StartProfilingOnMemoryInfraThread,
                                base::Unretained(this), mode, stack_mode,
                                sampling_rate, std::move(closure)));
}

void Supervisor::StartProfilingOnMemoryInfraThread(Mode mode,
                                                   mojom::StackMode stack_mode,
                                                   uint32_t sampling_rate,
                                                   base::OnceClosure closure) {
  mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper> helper;
  mojo::PendingRemote<memory_instrumentation::mojom::HeapProfiler> profiler;
  auto profiler_receiver = profiler.InitWithNewPipeAndPassReceiver();
  content::GetMemoryInstrumentationRegistry()->RegisterHeapProfiler(
      std::move(profiler), helper.InitWithNewPipeAndPassReceiver());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Supervisor::StartServiceOnIOThread,
                     base::Unretained(this), std::move(profiler_receiver),
                     std::move(helper), mode, stack_mode, sampling_rate,
                     std::move(closure)));
}

Mode Supervisor::GetMode() {
  DCHECK(HasStarted());
  return client_connection_manager_->GetMode();
}

void Supervisor::StartManualProfiling(
    base::ProcessId pid,
    mojom::ProfilingService::AddProfilingClientCallback
        started_profiling_closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(HasStarted());
  client_connection_manager_->StartProfilingProcess(
      pid, std::move(started_profiling_closure));
}

void Supervisor::GetProfiledPids(GetProfiledPidsCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(HasStarted());

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Supervisor::GetProfiledPidsOnIOThread,
                                base::Unretained(this), std::move(callback)));
}

uint32_t Supervisor::GetSamplingRate() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(HasStarted());

  return controller_->sampling_rate();
}

void Supervisor::RequestTraceWithHeapDump(TraceFinishedCallback callback,
                                          bool anonymize) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(HasStarted());

  if (content::TracingController::GetInstance()->IsTracing()) {
    DLOG(ERROR) << "Requesting heap dump when tracing has already started.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, std::string()));
    return;
  }

  auto finished_dump_callback = base::BindOnce(
      [](TraceFinishedCallback callback, bool anonymize, bool success,
         uint64_t dump_guid) {
        // Once the trace has stopped, run |callback| on the UI thread.
        auto finish_sink_callback = base::BindOnce(
            [](TraceFinishedCallback callback,
               std::unique_ptr<std::string> in) {
              std::string result;
              result.swap(*in);
              content::GetUIThreadTaskRunner({})->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), true, std::move(result)));
            },
            std::move(callback));
        scoped_refptr<content::TracingController::TraceDataEndpoint> sink =
            content::TracingController::CreateStringEndpoint(
                std::move(finish_sink_callback));
        content::TracingController::GetInstance()->StopTracing(
            sink,
            /*agent_label=*/"", anonymize);
      },
      std::move(callback), anonymize);

  auto trigger_memory_dump_callback = base::BindOnce(
      [](base::OnceCallback<void(bool success, uint64_t dump_guid)>
             finished_dump_callback) {
        memory_instrumentation::MemoryInstrumentation::GetInstance()
            ->RequestGlobalDumpAndAppendToTrace(
                base::trace_event::MemoryDumpType::kExplicitlyTriggered,
                base::trace_event::MemoryDumpLevelOfDetail::kBackground,
                base::trace_event::MemoryDumpDeterminism::kNone,
                std::move(finished_dump_callback));
      },
      std::move(finished_dump_callback));

  // The only reason this should return false is if tracing is already enabled,
  // which we've already checked.
  bool result = content::TracingController::GetInstance()->StartTracing(
      GetBackgroundTracingConfig(anonymize),
      std::move(trigger_memory_dump_callback));
  DCHECK(result);
}

void Supervisor::StartServiceOnIOThread(
    mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfiler> receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper>
        remote_helper,
    Mode mode,
    mojom::StackMode stack_mode,
    uint32_t sampling_rate,
    base::OnceClosure closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  mojo::PendingRemote<mojom::ProfilingService> service =
      LaunchService(std::move(receiver), std::move(remote_helper));

  controller_ = std::make_unique<Controller>(std::move(service), stack_mode,
                                             sampling_rate);
  base::WeakPtr<Controller> controller_weak_ptr = controller_->GetWeakPtr();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Supervisor::FinishInitializationOnUIhread,
                                base::Unretained(this), mode,
                                std::move(closure), controller_weak_ptr));
}

void Supervisor::FinishInitializationOnUIhread(
    Mode mode,
    base::OnceClosure closure,
    base::WeakPtr<Controller> controller_weak_ptr) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!started_);
  started_ = true;

  if (constructor_) {
    client_connection_manager_ = (*constructor_)(controller_weak_ptr, mode);
  } else {
    client_connection_manager_ =
        std::make_unique<ClientConnectionManager>(controller_weak_ptr, mode);
  }

  client_connection_manager_->Start();
  if (closure)
    std::move(closure).Run();
}

void Supervisor::GetProfiledPidsOnIOThread(GetProfiledPidsCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  auto post_result_to_ui_thread = base::BindOnce(
      [](GetProfiledPidsCallback callback,
         const std::vector<base::ProcessId>& result) {
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), result));
      },
      std::move(callback));
  controller_->GetProfiledPids(std::move(post_result_to_ui_thread));
}

}  // namespace heap_profiling
