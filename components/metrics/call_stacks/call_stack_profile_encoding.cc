// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/call_stack_profile_encoding.h"

#include "base/notreached.h"
#include "base/profiler/call_stack_profile_params.h"
#include "base/profiler/process_type.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

Process ToExecutionContextProcess(base::ProfilerProcessType process) {
  switch (process) {
    case base::ProfilerProcessType::kUnknown:
      return UNKNOWN_PROCESS;
    case base::ProfilerProcessType::kBrowser:
      return BROWSER_PROCESS;
    case base::ProfilerProcessType::kRenderer:
      return RENDERER_PROCESS;
    case base::ProfilerProcessType::kGpu:
      return GPU_PROCESS;
    case base::ProfilerProcessType::kUtility:
      return UTILITY_PROCESS;
    case base::ProfilerProcessType::kNetworkService:
      return NETWORK_SERVICE_PROCESS;
    case base::ProfilerProcessType::kZygote:
      return ZYGOTE_PROCESS;
    case base::ProfilerProcessType::kSandboxHelper:
      return SANDBOX_HELPER_PROCESS;
    case base::ProfilerProcessType::kPpapiPlugin:
      return PPAPI_PLUGIN_PROCESS;
  }
  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_PROCESS;
}

Thread ToExecutionContextThread(base::ProfilerThreadType thread) {
  switch (thread) {
    case base::ProfilerThreadType::kUnknown:
      return UNKNOWN_THREAD;
    case base::ProfilerThreadType::kMain:
      return MAIN_THREAD;
    case base::ProfilerThreadType::kIo:
      return IO_THREAD;
    case base::ProfilerThreadType::kCompositor:
      return COMPOSITOR_THREAD;
    case base::ProfilerThreadType::kServiceWorker:
      return SERVICE_WORKER_THREAD;
  }
  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_THREAD;
}

SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    base::CallStackProfileParams::Trigger trigger) {
  switch (trigger) {
    case base::CallStackProfileParams::Trigger::kUnknown:
      return SampledProfile::UNKNOWN_TRIGGER_EVENT;
    case base::CallStackProfileParams::Trigger::kProcessStartup:
      return SampledProfile::PROCESS_STARTUP;
    case base::CallStackProfileParams::Trigger::kJankyTask:
      return SampledProfile::JANKY_TASK;
    case base::CallStackProfileParams::Trigger::kThreadHung:
      return SampledProfile::THREAD_HUNG;
    case base::CallStackProfileParams::Trigger::kPeriodicCollection:
      return SampledProfile::PERIODIC_COLLECTION;
    case base::CallStackProfileParams::Trigger::kPeriodicHeapCollection:
      return SampledProfile::PERIODIC_HEAP_COLLECTION;
  }
  NOTREACHED_IN_MIGRATION();
  return SampledProfile::UNKNOWN_TRIGGER_EVENT;
}

}  // namespace metrics
