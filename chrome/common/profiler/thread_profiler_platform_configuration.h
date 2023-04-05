// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_
#define CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_

#include <memory>

#include "components/metrics/call_stack_profile_params.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
// The absl::optional<version_info::Channel> release_channel passed to functions
// in this interface should be the channel for released Chrome and nullopt for
// development/CQ builds.
class ThreadProfilerPlatformConfiguration {
 public:
  // The relative populations to use for enabling/disabling the profiler.
  // |enabled| + |experiment| is expected to equal 100. Profiling is to be
  // enabled with probability |enabled|/100. The fraction |experiment|/100 is to
  // be split in to two equal-sized experiment groups with probability
  // |experiment|/(2 * 100), one of which will be enabled and one disabled.
  struct RelativePopulations {
    int enabled;
    int experiment;

    // TODO(crbug.com/1430519): Remove this field when the DAU shift is
    // explained.
    // Add a third group periodic_only to the experiment. |experiment| must be
    // divisible by 3 when this flag is true.
    bool add_periodic_only_group = false;
  };

  virtual ~ThreadProfilerPlatformConfiguration() = default;

  // Create the platform configuration.
  static std::unique_ptr<ThreadProfilerPlatformConfiguration> Create(
      bool browser_test_mode_enabled);

  // True if the platform supports the StackSamplingProfiler and the profiler is
  // to be run for the released Chrome channel or development/CQ build.
  bool IsSupported(absl::optional<version_info::Channel> release_channel) const;

  // Returns the relative population disposition for the released Chrome channel
  // or development/CQ build on the platform. See the documentation on
  // RelativePopulations. Enable rates are valid only if IsSupported().
  virtual RelativePopulations GetEnableRates(
      absl::optional<version_info::Channel> release_channel) const = 0;

  // Returns the fraction of the time that profiling should be randomly enabled
  // for the child |process|. The return value is in the range [0.0, 1.0].
  virtual double GetChildProcessEnableFraction(
      metrics::CallStackProfileParams::Process process) const = 0;

  // Returns whether the profiler is enabled for |thread| in |process|.
  virtual bool IsEnabledForThread(
      metrics::CallStackProfileParams::Process process,
      metrics::CallStackProfileParams::Thread thread) const = 0;

 protected:
  // True if the profiler is to be run for the released Chrome channel or
  // development/CQ build on the platform. Does not need to check whether the
  // StackSamplingProfiler is supported on the platform since that's done in
  // IsSupported().
  virtual bool IsSupportedForChannel(
      absl::optional<version_info::Channel> release_channel) const = 0;
};

#endif  // CHROME_COMMON_PROFILER_THREAD_PROFILER_PLATFORM_CONFIGURATION_H_
