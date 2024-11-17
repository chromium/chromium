// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_CHROME_THREAD_GROUP_PROFILER_CLIENT_H_
#define CHROME_COMMON_PROFILER_CHROME_THREAD_GROUP_PROFILER_CLIENT_H_

#include "base/functional/callback.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/thread_group_profiler_client.h"
#include "components/sampling_profiler/process_type.h"

namespace base {
class CommandLine;
class ProfileBuilder;
}  // namespace base

// ChromeThreadGroupProfilerClient implements the ThreadGroupProfilerClient
// interface for the Chrome browser, providing functionality to control and
// configure thread group profiling behavior.
class ChromeThreadGroupProfilerClient : public base::ThreadGroupProfilerClient {
 public:
  ChromeThreadGroupProfilerClient() = default;
  ChromeThreadGroupProfilerClient(const ChromeThreadGroupProfilerClient&) =
      delete;
  ChromeThreadGroupProfilerClient& operator=(
      const ChromeThreadGroupProfilerClient&) = delete;

  // base::ThreadGroupProfilerClient implementation:
  base::StackSamplingProfiler::SamplingParams GetSamplingParams() override;
  std::unique_ptr<base::ProfileBuilder> CreateProfileBuilder(
      base::OnceClosure builder_completed_callback) override;
  base::StackSamplingProfiler::UnwindersFactory GetUnwindersFactory() override;
  bool IsProfilerEnabledForCurrentProcess() override;
  bool IsSingleProcess(const base::CommandLine& command_line) override;
  sampling_profiler::ProfilerProcessType GetProcessType();
};

#endif  // CHROME_COMMON_PROFILER_CHROME_THREAD_GROUP_PROFILER_CLIENT_H_
