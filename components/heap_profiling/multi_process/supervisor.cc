// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/multi_process/supervisor.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
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

base::trace_event::TraceConfig GetBackgroundTracingConfig() {
  // Disable all categories other than memory-infra.
  base::trace_event::TraceConfig trace_config(
      "-*,disabled-by-default-memory-infra",
      base::trace_event::RECORD_UNTIL_FULL);

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
  NOTREACHED();
}

bool Supervisor::HasStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  DCHECK(!started_);
  if (initialization_state_ == InitializationState::kInitialized) {
    StartClientConnectionOnUIThread(mode, std::move(closure));
    return;
  }

  CHECK(initialization_state_ == InitializationState::kNotInitialized);
  initialization_state_ = InitializationState::kInitializing;

  // Unretained is safe because `this` is a leaked global singleton.
  auto ui_thread_callback = base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&Supervisor::ControllerStartedOnUIThread,
                     base::Unretained(this), mode, std::move(closure)));
  auto io_thread_callback = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&Supervisor::LaunchServiceOnIOThread,
                     base::Unretained(this), stack_mode, sampling_rate,
                     std::move(ui_thread_callback)));
  base::trace_event::MemoryDumpManager::GetInstance()
      ->GetDumpThreadTaskRunner()
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &Supervisor::RegisterProfilerOnMemoryInfraThread,
                     base::Unretained(this), std::move(io_thread_callback)));
}

void Supervisor::Stop(base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  if (initialization_state_ == InitializationState::kInitializing) {
    auto retry_stop_on_ui = base::BindPostTask(
        content::GetUIThreadTaskRunner({}),
        base::BindOnce(&Supervisor::Stop, base::Unretained(this),
                       std::move(callback)));

    auto hop_to_io = base::BindPostTask(content::GetIOThreadTaskRunner({}),
                                        std::move(retry_stop_on_ui));

    // Post to MemoryInfra thread to queue behind the initial Start task.
    base::trace_event::MemoryDumpManager::GetInstance()
        ->GetDumpThreadTaskRunner()
        ->PostTask(FROM_HERE, std::move(hop_to_io));
    return;
  }

  if (!started_) {
    std::move(callback).Run(false);
    return;
  }

  started_ = false;

  // Stop observing/connecting new processes immediately while shutdown is in
  // flight.
  client_connection_manager_.reset();

  auto on_stopped_on_ui = base::BindPostTask(content::GetUIThreadTaskRunner({}),
                                             std::move(callback));

  // Unretained is safe because `this` aka Supervisor is leaked singleton
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Supervisor::StopOnIOThread, base::Unretained(this),
                     std::move(on_stopped_on_ui)));
}

void Supervisor::RegisterProfilerOnMemoryInfraThread(
    base::OnceCallback<void(
        mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfiler>,
        mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper>)>
        continue_on_io_thread) {
  mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper> helper;
  mojo::PendingRemote<memory_instrumentation::mojom::HeapProfiler> profiler;
  auto profiler_receiver = profiler.InitWithNewPipeAndPassReceiver();
  content::GetMemoryInstrumentationRegistry()->RegisterHeapProfiler(
      std::move(profiler), helper.InitWithNewPipeAndPassReceiver());
  std::move(continue_on_io_thread)
      .Run(std::move(profiler_receiver), std::move(helper));
}

Mode Supervisor::GetMode() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  DCHECK(HasStarted());
  return client_connection_manager_->GetMode();
}

void Supervisor::StartManualProfiling(
    base::ProcessId pid,
    mojom::ProfilingService::AddProfilingClientCallback
        started_profiling_closure) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  DCHECK(HasStarted());
  client_connection_manager_->StartProfilingProcess(
      pid, std::move(started_profiling_closure));
}

void Supervisor::GetProfiledPids(GetProfiledPidsCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  DCHECK(HasStarted());

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Supervisor::GetProfiledPidsOnIOThread,
                                base::Unretained(this), std::move(callback)));
}

uint32_t Supervisor::GetSamplingRate() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  DCHECK(HasStarted());

  return controller_->sampling_rate();
}

void Supervisor::RequestTraceWithHeapDump(TraceFinishedCallback callback,
                                          bool anonymize) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  DCHECK(HasStarted());

  if (content::TracingController::GetInstance()->IsTracing()) {
    DLOG(ERROR) << "Requesting heap dump when tracing has already started.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, std::string()));
    return;
  }

  auto finished_dump_callback = base::BindOnce(
      [](TraceFinishedCallback callback,
         memory_instrumentation::mojom::RequestOutcome outcome,
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
            /*agent_label=*/"");
      },
      std::move(callback));

  auto trigger_memory_dump_callback = base::BindOnce(
      [](base::OnceCallback<void(
             memory_instrumentation::mojom::RequestOutcome outcome,
             uint64_t dump_guid)> finished_dump_callback) {
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
      GetBackgroundTracingConfig(), std::move(trigger_memory_dump_callback),
      anonymize);
  DCHECK(result);
}

void Supervisor::LaunchServiceOnIOThread(
    mojom::StackMode stack_mode,
    uint32_t sampling_rate,
    base::OnceCallback<void(base::WeakPtr<Controller>)> continue_on_ui_thread,
    mojo::PendingReceiver<memory_instrumentation::mojom::HeapProfiler> receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::HeapProfilerHelper>
        remote_helper) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  mojo::PendingRemote<mojom::ProfilingService> service =
      LaunchService(std::move(receiver), std::move(remote_helper));

  controller_ = std::make_unique<Controller>(std::move(service), stack_mode,
                                             sampling_rate);
  std::move(continue_on_ui_thread).Run(controller_->GetWeakPtr());
}

void Supervisor::ControllerStartedOnUIThread(
    Mode mode,
    base::OnceClosure closure,
    base::WeakPtr<Controller> controller_weak_ptr) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  CHECK(initialization_state_ == InitializationState::kInitializing);
  initialization_state_ = InitializationState::kInitialized;
  controller_weak_ptr_ = controller_weak_ptr;
  StartClientConnectionOnUIThread(mode, std::move(closure));
}

void Supervisor::StartClientConnectionOnUIThread(Mode mode,
                                                 base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  DCHECK(!started_);
  started_ = true;

  if (constructor_) {
    client_connection_manager_ = (*constructor_)(controller_weak_ptr_, mode);
  } else {
    client_connection_manager_ =
        std::make_unique<ClientConnectionManager>(controller_weak_ptr_, mode);
  }

  client_connection_manager_->Start();
  if (closure) {
    std::move(closure).Run();
  }
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

void Supervisor::StopOnIOThread(base::OnceCallback<void(bool)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (!controller_) {
    std::move(callback).Run(false);
    return;
  }

  controller_->StopProfilingAllClients(std::move(callback));
}


}  // namespace heap_profiling
