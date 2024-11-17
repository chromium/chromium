// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/chrome_thread_profiler_client.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/work_id_provider.h"
#include "base/process/process.h"
#include "base/profiler/sample_metadata.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/common/profiler/unwind_util.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/sampling_profiler/process_type.h"
#include "content/public/common/content_switches.h"

base::StackSamplingProfiler::SamplingParams
ChromeThreadProfilerClient::GetSamplingParams() {
  return ThreadProfilerConfiguration::Get()->GetSamplingParams();
}

std::unique_ptr<base::ProfileBuilder>
ChromeThreadProfilerClient::CreateProfileBuilder(
    sampling_profiler::CallStackProfileParams profile_params,
    metrics::WorkIdRecorder* work_id_recorder,
    base::OnceClosure builder_completed_callback) {
  return std::make_unique<metrics::CallStackProfileBuilder>(
      profile_params, work_id_recorder, std::move(builder_completed_callback));
}

base::StackSamplingProfiler::UnwindersFactory
ChromeThreadProfilerClient::GetUnwindersFactory() {
  return CreateCoreUnwindersFactory();
}

bool ChromeThreadProfilerClient::IsProfilerEnabledForCurrentProcessAndThread(
    sampling_profiler::ProfilerThreadType thread) {
  return ThreadProfilerConfiguration::Get()
      ->IsProfilerEnabledForCurrentProcessAndThread(thread);
}

sampling_profiler::ProfilerProcessType
ChromeThreadProfilerClient::GetProcessType(
    const base::CommandLine& command_line) {
  return GetProfilerProcessType(command_line);
}

bool ChromeThreadProfilerClient::IsSingleProcess(
    const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kSingleProcess) ||
         command_line.HasSwitch(switches::kInProcessGPU);
}
