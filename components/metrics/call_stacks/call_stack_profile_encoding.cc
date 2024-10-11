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
  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_PROCESS;
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
  }
  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_THREAD;
}

SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    sampling_profiler::CallStackProfileParams::Trigger trigger) {
  switch (trigger) {
    case sampling_profiler::CallStackProfileParams::Trigger::kUnknown:
      return SampledProfile::UNKNOWN_TRIGGER_EVENT;
    case sampling_profiler::CallStackProfileParams::Trigger::kProcessStartup:
      return SampledProfile::PROCESS_STARTUP;
    case sampling_profiler::CallStackProfileParams::Trigger::kJankyTask:
      return SampledProfile::JANKY_TASK;
    case sampling_profiler::CallStackProfileParams::Trigger::kThreadHung:
      return SampledProfile::THREAD_HUNG;
    case sampling_profiler::CallStackProfileParams::Trigger::
        kPeriodicCollection:
      return SampledProfile::PERIODIC_COLLECTION;
    case sampling_profiler::CallStackProfileParams::Trigger::
        kPeriodicHeapCollection:
      return SampledProfile::PERIODIC_HEAP_COLLECTION;
  }
  NOTREACHED_IN_MIGRATION();
  return SampledProfile::UNKNOWN_TRIGGER_EVENT;
}

}  // namespace metrics
