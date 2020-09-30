// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_THREAD_PROFILER_CONFIGURATION_H_
#define CHROME_COMMON_PROFILER_THREAD_PROFILER_CONFIGURATION_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/call_stack_profile_params.h"

namespace base {
class CommandLine;
}  // namespace base

class ThreadProfilerPlatformConfiguration;

// ThreadProfilerConfiguration chooses a configuration for the enable state of
// the stack sampling profiler across all processes. This configuration is
// determined once at browser process startup. Configurations for child
// processes are communicated via command line arguments.
class ThreadProfilerConfiguration {
 public:
  ThreadProfilerConfiguration();
  ~ThreadProfilerConfiguration();

  // Get the stack sampling params to use.
  base::StackSamplingProfiler::SamplingParams GetSamplingParams() const;

  // True if the profiler is enabled for any thread in the current process.
  bool IsProfilerEnabledForCurrentProcess() const;

  // True if the profiler should be started for |thread| in the current process.
  bool IsProfilerEnabledForCurrentProcessAndThread(
      metrics::CallStackProfileParams::Thread thread) const;

  // Get the synthetic field trial configuration. Returns true if a synthetic
  // field trial should be registered. This should only be called from the
  // browser process. When run at startup, the profiler must use a synthetic
  // field trial since it runs before the metrics field trials are initialized.
  bool GetSyntheticFieldTrial(std::string* trial_name,
                              std::string* group_name) const;

  // Add a command line switch that instructs the child process to run the
  // profiler. This should only be called from the browser process.
  void AppendCommandLineSwitchForChildProcess(
      base::CommandLine* command_line) const;

  // Returns the ThreadProfilerConfiguration for the process.
  static ThreadProfilerConfiguration* Get();

 private:
  // Configuration to use for this Chrome instance.
  enum ProfileConfiguration {
    // Chrome-wide configurations set in the browser process.
    PROFILE_DISABLED,
    PROFILE_DISABLED_MODULE_NOT_INSTALLED,
    PROFILE_CONTROL,
    PROFILE_ENABLED,

    // Configuration set in the child processes, which receive their enable
    // state on the command line from the browser process.
    PROFILE_FROM_COMMAND_LINE
  };

  // Configuration variations, along with weights to use when randomly choosing
  // one of a set of variations.
  struct Variation {
    ProfileConfiguration config;
    int weight;
  };

  // Randomly chooses a configuration from the weighted variations. Weights are
  // expected to sum to 100 as a sanity check.
  static ProfileConfiguration ChooseConfiguration(
      const std::vector<Variation>& variations);

  // Generates sampling profiler configurations for all processes.
  static ProfileConfiguration GenerateConfiguration(
      metrics::CallStackProfileParams::Process process,
      const ThreadProfilerPlatformConfiguration& platform_configuration);

  // NOTE: all state in this class must be const and initialized at construction
  // time to ensure thread-safe access post-construction.

  // The currently-executing process.
  const metrics::CallStackProfileParams::Process current_process_;

  // Platform-dependent configuration upon which |configuration_| is based.
  const std::unique_ptr<ThreadProfilerPlatformConfiguration>
      platform_configuration_;

  // In the browser process this represents the configuration to use across all
  // Chrome processes. In the child processes it is always
  // PROFILE_FROM_COMMAND_LINE.
  const ProfileConfiguration configuration_;

  DISALLOW_COPY_AND_ASSIGN(ThreadProfilerConfiguration);
};

#endif  // CHROME_COMMON_PROFILER_THREAD_PROFILER_CONFIGURATION_H_
