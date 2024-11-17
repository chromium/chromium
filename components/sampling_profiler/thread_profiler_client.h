// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAMPLING_PROFILER_THREAD_PROFILER_CLIENT_H_
#define COMPONENTS_SAMPLING_PROFILER_THREAD_PROFILER_CLIENT_H_

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/sampling_profiler/call_stack_profile_params.h"
#include "components/sampling_profiler/process_type.h"

namespace metrics {
class WorkIdRecorder;
}

namespace sampling_profiler {

// Interface for a client to participate in thread profiling. The primary use
// for this API is for embedders to provide configuration to control the
// profiling, and consume profiling results.
//
// Functions on this interface are invoked on multiple threads without
// synchronization. Virtual function overrides in subclasses must be
// thread-safe.
class ThreadProfilerClient {
 public:
  ThreadProfilerClient() = default;

  ThreadProfilerClient(const ThreadProfilerClient&) = delete;
  ThreadProfilerClient& operator=(const ThreadProfilerClient&) = delete;

  virtual ~ThreadProfilerClient() = default;

  // Gets the parameters to control the sampling for a new SamplingProfiler
  // instance.
  virtual base::StackSamplingProfiler::SamplingParams GetSamplingParams() = 0;

  // Creates a `base::ProfileBuilder` instance to record profiles for a new
  // SamplingProfiler instance.
  virtual std::unique_ptr<base::ProfileBuilder> CreateProfileBuilder(
      CallStackProfileParams profile_params,
      metrics::WorkIdRecorder* work_id_recorder,
      base::OnceClosure builder_completed_callback) = 0;

  // Gets the factory function for providing unwinders to a new SamplingProfiler
  // instance.
  virtual base::StackSamplingProfiler::UnwindersFactory
  GetUnwindersFactory() = 0;

  // Indicates if the embedder has enabled profiling for this specific process
  // and thread. The embedder may enable (or disable) profiling based on
  // platform, subset of executions, application version, etc.
  virtual bool IsProfilerEnabledForCurrentProcessAndThread(
      ProfilerThreadType thread) = 0;

  // Determines the process type of the current process, primarily based on the
  // command line switches.
  virtual ProfilerProcessType GetProcessType(
      const base::CommandLine& command_line) = 0;

  // Determines if the embedder is running in single process mode, primarily
  // based on the command line switches.
  virtual bool IsSingleProcess(const base::CommandLine& command_line) = 0;
};

}  // namespace sampling_profiler

#endif  // COMPONENTS_SAMPLING_PROFILER_THREAD_PROFILER_CLIENT_H_
