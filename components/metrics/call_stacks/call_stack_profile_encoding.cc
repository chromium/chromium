// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/call_stack_profile_encoding.h"
#include "base/notreached.h"

namespace metrics {

Process ToExecutionContextProcess(CallStackProfileParams::Process process) {
  switch (process) {
    case CallStackProfileParams::Process::kUnknown:
      return UNKNOWN_PROCESS;
    case CallStackProfileParams::Process::kBrowser:
      return BROWSER_PROCESS;
    case CallStackProfileParams::Process::kRenderer:
      return RENDERER_PROCESS;
    case CallStackProfileParams::Process::kGpu:
      return GPU_PROCESS;
    case CallStackProfileParams::Process::kUtility:
      return UTILITY_PROCESS;
    case CallStackProfileParams::Process::kNetworkService:
      return NETWORK_SERVICE_PROCESS;
    case CallStackProfileParams::Process::kZygote:
      return ZYGOTE_PROCESS;
    case CallStackProfileParams::Process::kSandboxHelper:
      return SANDBOX_HELPER_PROCESS;
    case CallStackProfileParams::Process::kPpapiPlugin:
      return PPAPI_PLUGIN_PROCESS;
  }
  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_PROCESS;
}

Thread ToExecutionContextThread(CallStackProfileParams::Thread thread) {
  switch (thread) {
    case CallStackProfileParams::Thread::kUnknown:
      return UNKNOWN_THREAD;
    case CallStackProfileParams::Thread::kMain:
      return MAIN_THREAD;
    case CallStackProfileParams::Thread::kIo:
      return IO_THREAD;
    case CallStackProfileParams::Thread::kCompositor:
      return COMPOSITOR_THREAD;
    case CallStackProfileParams::Thread::kServiceWorker:
      return SERVICE_WORKER_THREAD;
  }
  NOTREACHED_IN_MIGRATION();
  return UNKNOWN_THREAD;
}

SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    CallStackProfileParams::Trigger trigger) {
  switch (trigger) {
    case CallStackProfileParams::Trigger::kUnknown:
      return SampledProfile::UNKNOWN_TRIGGER_EVENT;
    case CallStackProfileParams::Trigger::kProcessStartup:
      return SampledProfile::PROCESS_STARTUP;
    case CallStackProfileParams::Trigger::kJankyTask:
      return SampledProfile::JANKY_TASK;
    case CallStackProfileParams::Trigger::kThreadHung:
      return SampledProfile::THREAD_HUNG;
    case CallStackProfileParams::Trigger::kPeriodicCollection:
      return SampledProfile::PERIODIC_COLLECTION;
    case CallStackProfileParams::Trigger::kPeriodicHeapCollection:
      return SampledProfile::PERIODIC_HEAP_COLLECTION;
  }
  NOTREACHED_IN_MIGRATION();
  return SampledProfile::UNKNOWN_TRIGGER_EVENT;
}

}  // namespace metrics
