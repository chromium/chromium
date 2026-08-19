// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/call_stack_profile_encoding.h"

#include "base/notreached.h"
#include "components/sampling_profiler/call_stack_profile_params.h"
#include "components/sampling_profiler/process_type.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

Process ToExecutionContextProcess(
    sampling_profiler::ProfilerProcessType process) {
  switch (process) {
    case sampling_profiler::ProfilerProcessType::kUnknown:
      return UNKNOWN_PROCESS;
    case sampling_profiler::ProfilerProcessType::kBrowser:
      return BROWSER_PROCESS;
    case sampling_profiler::ProfilerProcessType::kRenderer:
      return RENDERER_PROCESS;
    case sampling_profiler::ProfilerProcessType::kGpu:
      return GPU_PROCESS;
    case sampling_profiler::ProfilerProcessType::kUtility:
      return UTILITY_PROCESS;
    case sampling_profiler::ProfilerProcessType::kNetworkService:
      return NETWORK_SERVICE_PROCESS;
    case sampling_profiler::ProfilerProcessType::kZygote:
      return ZYGOTE_PROCESS;
    case sampling_profiler::ProfilerProcessType::kSandboxHelper:
      return SANDBOX_HELPER_PROCESS;
    case sampling_profiler::ProfilerProcessType::kPpapiPlugin:
      return PPAPI_PLUGIN_PROCESS;
  }
  NOTREACHED();
}

Thread ToExecutionContextThread(sampling_profiler::ProfilerThreadType thread) {
  switch (thread) {
    case sampling_profiler::ProfilerThreadType::kUnknown:
      return UNKNOWN_THREAD;
    case sampling_profiler::ProfilerThreadType::kMain:
      return MAIN_THREAD;
    case sampling_profiler::ProfilerThreadType::kIo:
      return IO_THREAD;
    case sampling_profiler::ProfilerThreadType::kCompositor:
      return COMPOSITOR_THREAD;
    case sampling_profiler::ProfilerThreadType::kServiceWorker:
      return SERVICE_WORKER_THREAD;
    case sampling_profiler::ProfilerThreadType::kThreadPoolWorker:
      return THREAD_POOL_THREAD;
    case sampling_profiler::ProfilerThreadType::kNetwork:
      return NETWORK_THREAD;
    case sampling_profiler::ProfilerThreadType::kDisplayCompositorGpu:
      return DISPLAY_COMPOSITOR_GPU_THREAD;
  }
  NOTREACHED();
}

SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    mojom::TriggerEvent trigger_event) {
  switch (trigger_event) {
    case mojom::TriggerEvent::kUnknown:
      return SampledProfile::UNKNOWN_TRIGGER_EVENT;
    case mojom::TriggerEvent::kPeriodicCollection:
      return SampledProfile::PERIODIC_COLLECTION;
    case mojom::TriggerEvent::kResumeFromSuspend:
      return SampledProfile::RESUME_FROM_SUSPEND;
    case mojom::TriggerEvent::kRestoreSession:
      return SampledProfile::RESTORE_SESSION;
    case mojom::TriggerEvent::kProcessStartup:
      return SampledProfile::PROCESS_STARTUP;
    case mojom::TriggerEvent::kJankyTask:
      return SampledProfile::JANKY_TASK;
    case mojom::TriggerEvent::kThreadHung:
      return SampledProfile::THREAD_HUNG;
    case mojom::TriggerEvent::kPeriodicHeapCollection:
      return SampledProfile::PERIODIC_HEAP_COLLECTION;
    case mojom::TriggerEvent::kPeriodicHeapChurnCollection:
      return SampledProfile::PERIODIC_HEAP_CHURN_COLLECTION;
  }
  NOTREACHED();
}

mojom::TriggerEvent ToMojomTriggerEvent(
    SampledProfile::TriggerEvent trigger_event) {
  switch (trigger_event) {
    case SampledProfile::UNKNOWN_TRIGGER_EVENT:
      return mojom::TriggerEvent::kUnknown;
    case SampledProfile::PERIODIC_COLLECTION:
      return mojom::TriggerEvent::kPeriodicCollection;
    case SampledProfile::RESUME_FROM_SUSPEND:
      return mojom::TriggerEvent::kResumeFromSuspend;
    case SampledProfile::RESTORE_SESSION:
      return mojom::TriggerEvent::kRestoreSession;
    case SampledProfile::PROCESS_STARTUP:
      return mojom::TriggerEvent::kProcessStartup;
    case SampledProfile::JANKY_TASK:
      return mojom::TriggerEvent::kJankyTask;
    case SampledProfile::THREAD_HUNG:
      return mojom::TriggerEvent::kThreadHung;
    case SampledProfile::PERIODIC_HEAP_COLLECTION:
      return mojom::TriggerEvent::kPeriodicHeapCollection;
    case SampledProfile::PERIODIC_HEAP_CHURN_COLLECTION:
      return mojom::TriggerEvent::kPeriodicHeapChurnCollection;
  }
  NOTREACHED();
}

}  // namespace metrics
