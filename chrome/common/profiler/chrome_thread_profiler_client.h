// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_CHROME_THREAD_PROFILER_CLIENT_H_
#define CHROME_COMMON_PROFILER_CHROME_THREAD_PROFILER_CLIENT_H_

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/sampling_profiler/call_stack_profile_params.h"
#include "components/sampling_profiler/process_type.h"
#include "components/sampling_profiler/thread_profiler_client.h"

namespace metrics {
class WorkIdRecorder;
}

// Implements the ThreadProfilerClient interface to configure and control thread
// profiling for Chrome.
//
// Note: virtual function override implementations must be thread-safe to
// satisfy the `ThreadProfilerClient` interface requirements.
class ChromeThreadProfilerClient
    : public sampling_profiler::ThreadProfilerClient {
 public:
  ChromeThreadProfilerClient() = default;
  ~ChromeThreadProfilerClient() override = default;

  ChromeThreadProfilerClient(const ChromeThreadProfilerClient&) = delete;
  ChromeThreadProfilerClient& operator=(const ChromeThreadProfilerClient&) =
      delete;

  // sampling_profiler::ThreadProfilerClient implementation.
  base::StackSamplingProfiler::SamplingParams GetSamplingParams() override;
  std::unique_ptr<base::ProfileBuilder> CreateProfileBuilder(
      sampling_profiler::CallStackProfileParams profile_params,
      metrics::WorkIdRecorder* work_id_recorder,
      base::OnceClosure builder_completed_callback) override;
  base::StackSamplingProfiler::UnwindersFactory GetUnwindersFactory() override;
  bool IsProfilerEnabledForCurrentProcessAndThread(
      sampling_profiler::ProfilerThreadType thread) override;
  sampling_profiler::ProfilerProcessType GetProcessType(
      const base::CommandLine& command_line) override;
  bool IsSingleProcess(const base::CommandLine& command_line) override;
};

#endif  // CHROME_COMMON_PROFILER_CHROME_THREAD_PROFILER_CLIENT_H_
