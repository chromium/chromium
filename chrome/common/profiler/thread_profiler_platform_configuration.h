// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_
#define CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "components/sampling_profiler/process_type.h"
#include "components/version_info/channel.h"

// Encapsulates the platform-specific configuration for the ThreadProfiler.
//
// The interface functions this class make a distinction between 'supported' and
// 'enabled' state. Supported means the profiler can be run in *some*
// circumstances for *some* fraction of the population on the platform/{released
// Chrome channel, development/CQ build} combination. This state is intended to
// enable experiment reporting. This avoids spamming UMA with experiment state
// on platforms/channels where the profiler is not being run.
//
// Enabled means we chose to the run the profiler on at least some threads on a
// platform/{released Chrome channel, development/CQ build} combination that is
// configured for profiling. The overall enable/disable state should be reported
// to UMA in this case.
//
// The std::optional<version_info::Channel> release_channel passed to functions
// in this interface should be the channel for released Chrome and nullopt for
// development/CQ builds.
class ThreadProfilerPlatformConfiguration {
 public:
  // The relative populations to use for enabling/disabling the profiler.
  // |disabled| + |enabled| + |experiment| is expected to equal 100.
  // - Within the enabled population, profiling is always enabled.
  // - Within the disabled population, profiling is always disabled.
  // - The experiment population is further split down to N equal-sized
  // subgroups with different configurations. Often, N = 2, where one subgroup
  // is disabled and one is enabled, but it could be more than 2 if we're
  // experimenting with more specific details of how and where we enable the
  // profiler. |experiment| must be divisible by N.
  struct RelativePopulations {
    int disabled;
    int enabled;
    int experiment;
  };

  virtual ~ThreadProfilerPlatformConfiguration() = default;

  // Create the platform configuration.
  static std::unique_ptr<ThreadProfilerPlatformConfiguration> Create(
      bool browser_test_mode_enabled,
      base::RepeatingCallback<bool(double)> is_enabled_on_dev_callback =
          base::BindRepeating(IsEnabled));

  // True if the platform supports the StackSamplingProfiler and the profiler is
  // to be run for the released Chrome channel or development/CQ build.
  bool IsSupported(std::optional<version_info::Channel> release_channel) const;

  // Returns the relative population disposition for the released Chrome channel
  // or development/CQ build on the platform. See the documentation on
  // RelativePopulations. Enable rates are valid only if IsSupported().
  virtual RelativePopulations GetEnableRates(
      std::optional<version_info::Channel> release_channel) const = 0;

  // Returns the fraction of the time that profiling should be randomly enabled
  // for the child |process|. The return value is in the range [0.0, 1.0].
  virtual double GetChildProcessPerExecutionEnableFraction(
      sampling_profiler::ProfilerProcessType process) const = 0;

  // Choose a process to run profiling when profiling is enabled. Running
  // the sampler on a single process instead of all processes at the same time
  // will help reduce the impact on users. If std::nullopt is returned, the
  // setting can be ignored. All processes will be sampled.
  virtual std::optional<sampling_profiler::ProfilerProcessType>
  ChooseEnabledProcess() const = 0;

  // Returns whether the profiler is enabled for |thread| in |process|.
  virtual bool IsEnabledForThread(
      sampling_profiler::ProfilerProcessType process,
      sampling_profiler::ProfilerThreadType thread,
      std::optional<version_info::Channel> release_channel) const = 0;

 protected:
  // True if the profiler is to be run for the released Chrome channel or
  // development/CQ build on the platform. Does not need to check whether the
  // StackSamplingProfiler is supported on the platform since that's done in
  // IsSupported().
  virtual bool IsSupportedForChannel(
      std::optional<version_info::Channel> release_channel) const = 0;

 private:
  // Returns `true` with given `enabled_probability`, where
  // 0 <= `enabled_probability` <= 1.
  static bool IsEnabled(double enabled_probability);
};

#endif  // CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_
