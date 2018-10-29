// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/supervisor.h"

#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "components/heap_profiling/client_connection_manager.h"
#include "components/services/heap_profiling/public/cpp/controller.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/service_manager/public/cpp/connector.h"

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
  NOTREACHED();
}

bool Supervisor::HasStarted() {
  return started_;
}

void Supervisor::SetClientConnectionManagerConstructor(
    ClientConnectionManagerConstructor constructor) {
  DCHECK(!HasStarted());
  constructor_ = constructor;
}

void Supervisor::Start(content::ServiceManagerConnection* connection,
                       base::OnceClosure closure) {
  Start(connection, GetModeForStartup(), GetStackModeForStartup(),
        GetSamplingRateForStartup(), std::move(closure));
}

void Supervisor::Start(content::ServiceManagerConnection* connection,
                       Mode mode,
                       mojom::StackMode stack_mode,
                       uint32_t sampling_rate,
                       base::OnceClosure closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!started_);

  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&Supervisor::StartServiceOnIOThread,
                                base::Unretained(this),
                                connection->GetConnector()->Clone(), mode,
                                stack_mode, sampling_rate, std::move(closure)));
}

Mode Supervisor::GetMode() {
  DCHECK(HasStarted());
  return client_connection_manager_->GetMode();
}

void Supervisor::StartManualProfiling(base::ProcessId pid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(HasStarted());
  client_connection_manager_->StartProfilingProcess(pid);
}

void Supervisor::SetKeepSmallAllocations(bool keep_small_allocations) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(HasStarted());

  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&Supervisor::SetKeepSmallAllocationsOnIOThread,
                         base::Unretained(this), keep_small_allocations));
}

void Supervisor::GetProfiledPids(GetProfiledPidsCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(HasStarted());

  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&Supervisor::GetProfiledPidsOnIOThread,
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, std::string()));
    return;
  }

  auto finished_dump_callback = base::BindOnce(
      [](TraceFinishedCallback callback, bool success, uint64_t dump_guid) {
        // Once the trace has stopped, run |callback| on the UI thread.
        auto finish_sink_callback = base::Bind(
            [](TraceFinishedCallback callback,
               std::unique_ptr<const base::DictionaryValue> metadata,
               base::RefCountedString* in) {
              std::string result;
              result.swap(in->data());
              base::CreateSingleThreadTaskRunnerWithTraits(
                  {content::BrowserThread::UI})
                  ->PostTask(FROM_HERE,
                             base::BindOnce(std::move(callback), true,
                                            std::move(result)));
            },
            base::Passed(std::move(callback)));
        scoped_refptr<content::TracingController::TraceDataEndpoint> sink =
            content::TracingController::CreateStringEndpoint(
                std::move(finish_sink_callback));
        content::TracingController::GetInstance()->StopTracing(sink);
      },
      std::move(callback));

  auto trigger_memory_dump_callback = base::BindOnce(
      [](base::OnceCallback<void(bool success, uint64_t dump_guid)>
             finished_dump_callback) {
        memory_instrumentation::MemoryInstrumentation::GetInstance()
            ->RequestGlobalDumpAndAppendToTrace(
                base::trace_event::MemoryDumpType::EXPLICITLY_TRIGGERED,
                base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND,
                base::AdaptCallbackForRepeating(
                    std::move(finished_dump_callback)));
      },
      std::move(finished_dump_callback));

  // The only reason this should return false is if tracing is already enabled,
  // which we've already checked.
  // Use AdaptCallbackForRepeating since the argument passed to StartTracing()
  // is intended to be a OnceCallback, but the code has not yet been migrated.
  bool result = content::TracingController::GetInstance()->StartTracing(
      GetBackgroundTracingConfig(anonymize),
      base::AdaptCallbackForRepeating(std::move(trigger_memory_dump_callback)));
  DCHECK(result);
}

void Supervisor::StartServiceOnIOThread(
    std::unique_ptr<service_manager::Connector> connector,
    Mode mode,
    mojom::StackMode stack_mode,
    uint32_t sampling_rate,
    base::OnceClosure closure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  controller_.reset(
      new Controller(std::move(connector), stack_mode, sampling_rate));
  base::WeakPtr<Controller> controller_weak_ptr = controller_->GetWeakPtr();

  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&Supervisor::FinishInitializationOnUIhread,
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
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI})
            ->PostTask(FROM_HERE, base::BindOnce(std::move(callback), result));
      },
      std::move(callback));
  controller_->GetProfiledPids(std::move(post_result_to_ui_thread));
}

void Supervisor::SetKeepSmallAllocationsOnIOThread(
    bool keep_small_allocations) {
  controller_->SetKeepSmallAllocations(keep_small_allocations);
}

}  // namespace heap_profiling
