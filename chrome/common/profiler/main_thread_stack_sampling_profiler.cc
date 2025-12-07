// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/profiler/process_type.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"
#include "components/sampling_profiler/process_type.h"
#include "components/sampling_profiler/thread_profiler.h"
#include "content/public/common/content_switches.h"

namespace {

// Returns the profiler appropriate for the current process.
std::unique_ptr<sampling_profiler::ThreadProfiler> CreateThreadProfiler(
    const sampling_profiler::ProfilerProcessType process) {
  // TODO(wittman): Do this for other process types too.
  if (process == sampling_profiler::ProfilerProcessType::kBrowser) {
    metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
        base::BindRepeating(
            &metrics::CallStackProfileMetricsProvider::ReceiveProfile));
    return sampling_profiler::ThreadProfiler::CreateAndStartOnMainThread();
  }

  // No other processes are currently supported.
  return nullptr;
}

}  // namespace

MainThreadStackSamplingProfiler::MainThreadStackSamplingProfiler() {
  const sampling_profiler::ProfilerProcessType process =
      GetProfilerProcessType(*base::CommandLine::ForCurrentProcess());
  sampling_profiler_ = CreateThreadProfiler(process);
}

// Note that it's important for the |sampling_profiler_| destructor to run, as
// it ensures program correctness on shutdown. Without it, the profiler thread's
// destruction can race with the profiled thread's destruction, which results in
// the sampling thread attempting to profile the sampled thread after the
// sampled thread has already been shut down.
MainThreadStackSamplingProfiler::~MainThreadStackSamplingProfiler() = default;
