// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_THREAD_PROFILER_CONFIGURATION_H_
#define CHROME_COMMON_PROFILER_THREAD_PROFILER_CONFIGURATION_H_

#include <initializer_list>
#include <string>

#include "base/no_destructor.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/call_stack_profile_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
  // Returns the ThreadProfilerConfiguration for the process.
  static ThreadProfilerConfiguration* Get();

  ~ThreadProfilerConfiguration() = delete;
  ThreadProfilerConfiguration(const ThreadProfilerConfiguration&) = delete;
  ThreadProfilerConfiguration& operator=(const ThreadProfilerConfiguration&) =
      delete;

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

 private:
  friend base::NoDestructor<ThreadProfilerConfiguration>;

  // The variation groups that represent the Chrome-wide profiling
  // configurations.
  enum VariationGroup {
    // Disabled within the experiment.
    kProfileDisabled,

    // Disabled because the required module is not installed, and outside the
    // experiment.
    kProfileDisabledModuleNotInstalled,

    // Enabled within the experiment (and paired with equal-sized
    // kProfileDisabled group).
    kProfileControl,

    // Enabled outside of the experiment.
    kProfileEnabled,
  };

  // The configuration state for the browser process. If !has_value() profiling
  // is disabled and no variations state is reported. Otherwise profiling is
  // enabled based on the VariationGroup and the variation state is reported.
  using BrowserProcessConfiguration = absl::optional<VariationGroup>;

  // The configuration state in child processes.
  enum ChildProcessConfiguration {
    kChildProcessProfileDisabled,
    kChildProcessProfileEnabled,
  };

  // The configuration state for the current process, browser or child.
  using Configuration =
      absl::variant<BrowserProcessConfiguration, ChildProcessConfiguration>;

  // Configuration variations, along with weights to use when randomly choosing
  // one of a set of variations.
  struct Variation {
    VariationGroup group;
    int weight;
  };

  ThreadProfilerConfiguration();

  // True if the profiler is to be enabled for |variation_group|.
  static bool EnableForVariationGroup(
      absl::optional<VariationGroup> variation_group);

  // Randomly chooses a variation from the weighted variations. Weights are
  // expected to sum to 100 as a sanity check.
  static VariationGroup ChooseVariationGroup(
      std::initializer_list<Variation> variations);

  // Generates a configuration for the browser process.
  static BrowserProcessConfiguration GenerateBrowserProcessConfiguration(
      const ThreadProfilerPlatformConfiguration& platform_configuration);

  // Generates a configuration for a child process.
  static ChildProcessConfiguration GenerateChildProcessConfiguration(
      const base::CommandLine& command_line);

  // Generates a configuration for the current process.
  static Configuration GenerateConfiguration(
      metrics::CallStackProfileParams::Process process,
      const ThreadProfilerPlatformConfiguration& platform_configuration);

  // NOTE: all state in this class must be const and initialized at construction
  // time to ensure thread-safe access post-construction.

  // Platform-dependent configuration upon which |configuration_| is based.
  const std::unique_ptr<ThreadProfilerPlatformConfiguration>
      platform_configuration_;

  // Represents the configuration to use in the current process.
  const Configuration configuration_;
};

#endif  // CHROME_COMMON_PROFILER_THREAD_PROFILER_CONFIGURATION_H_
