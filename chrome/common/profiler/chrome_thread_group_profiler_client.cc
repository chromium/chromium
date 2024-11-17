// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/chrome_thread_group_profiler_client.h"

#include "base/command_line.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/common/profiler/unwind_util.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/sampling_profiler/call_stack_profile_params.h"
#include "components/sampling_profiler/process_type.h"
#include "content/public/common/content_switches.h"

base::StackSamplingProfiler::SamplingParams
ChromeThreadGroupProfilerClient::GetSamplingParams() {
  return ThreadProfilerConfiguration::Get()->GetSamplingParams();
}

std::unique_ptr<base::ProfileBuilder>
ChromeThreadGroupProfilerClient::CreateProfileBuilder(
    base::OnceClosure builder_completed_callback) {
  sampling_profiler::CallStackProfileParams profile_params{
      GetProcessType(),
      sampling_profiler::ProfilerThreadType::kThreadPoolWorker,
      sampling_profiler::CallStackProfileParams::Trigger::kPeriodicCollection};
  return std::make_unique<metrics::CallStackProfileBuilder>(
      profile_params, nullptr, std::move(builder_completed_callback));
}

base::StackSamplingProfiler::UnwindersFactory
ChromeThreadGroupProfilerClient::GetUnwindersFactory() {
  return CreateCoreUnwindersFactory();
}

bool ChromeThreadGroupProfilerClient::IsProfilerEnabledForCurrentProcess() {
  // Note: This implementation might need to be adjusted based on your specific
  // requirements for thread group profiling.
  return ThreadProfilerConfiguration::Get()
      ->IsProfilerEnabledForCurrentProcessAndThread(
          sampling_profiler::ProfilerThreadType::kThreadPoolWorker);
}

sampling_profiler::ProfilerProcessType
ChromeThreadGroupProfilerClient::GetProcessType() {
  return GetProfilerProcessType(*base::CommandLine::ForCurrentProcess());
}

bool ChromeThreadGroupProfilerClient::IsSingleProcess(
    const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kSingleProcess) ||
         command_line.HasSwitch(switches::kInProcessGPU);
}
