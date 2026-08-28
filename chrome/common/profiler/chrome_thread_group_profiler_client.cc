// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/chrome_thread_group_profiler_client.h"

#include "base/command_line.h"
#include "base/profiler/periodic_sampling_scheduler.h"
#include "base/time/time.h"
#include "chrome/common/profiler/core_unwinders.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/sampling_profiler/call_stack_profile_params.h"
#include "components/sampling_profiler/process_type.h"
#include "content/public/common/content_switches.h"

namespace {
// Fraction of execution time spent sampling.
// ThreadProfilerConfiguration::GetSamplingParams() specifies duration = 30s.
// With fraction = 0.02, this produces a sampling period of 30s/0.02 = 1500s =
// 25m.
constexpr double kFractionOfExecutionTimeToSample = 0.02;
}  // namespace

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

std::unique_ptr<base::PeriodicSamplingScheduler>
ChromeThreadGroupProfilerClient::CreatePeriodicSamplingScheduler() {
  const double fraction =
      ThreadProfilerConfiguration::Get()->IsBrowserTestModeEnabled()
          ? 1.0
          : kFractionOfExecutionTimeToSample;
  const base::StackSamplingProfiler::SamplingParams params =
      GetSamplingParams();
  const base::TimeDelta duration =
      params.sampling_interval * params.samples_per_profile;
  return std::make_unique<base::PeriodicSamplingScheduler>(
      duration, fraction, base::TimeTicks::Now());
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
