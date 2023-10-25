// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "chrome/common/profiler/process_type.h"

namespace {

// The default configuration to use in the absence of special circumstances on a
// specific platform.
class DefaultPlatformConfiguration
    : public ThreadProfilerPlatformConfiguration {
 public:
  explicit DefaultPlatformConfiguration(bool browser_test_mode_enabled);

  RelativePopulations GetEnableRates(
      absl::optional<version_info::Channel> release_channel) const override;

  double GetChildProcessPerExecutionEnableFraction(
      metrics::CallStackProfileParams::Process process) const override;

  absl::optional<metrics::CallStackProfileParams::Process>
  ChooseEnabledProcess() const override;

  bool IsEnabledForThread(
      metrics::CallStackProfileParams::Process process,
      metrics::CallStackProfileParams::Thread thread,
      absl::optional<version_info::Channel> release_channel) const override;

 protected:
  bool IsSupportedForChannel(
      absl::optional<version_info::Channel> release_channel) const override;

  bool browser_test_mode_enabled() const { return browser_test_mode_enabled_; }

 private:
  const bool browser_test_mode_enabled_;
};

DefaultPlatformConfiguration::DefaultPlatformConfiguration(
    bool browser_test_mode_enabled)
    : browser_test_mode_enabled_(browser_test_mode_enabled) {}

ThreadProfilerPlatformConfiguration::RelativePopulations
DefaultPlatformConfiguration::GetEnableRates(
    absl::optional<version_info::Channel> release_channel) const {
  CHECK(IsSupportedForChannel(release_channel));

  if (!release_channel) {
    // This is a local/CQ build.
    return RelativePopulations{100, 0};
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (browser_test_mode_enabled()) {
    // This is a browser test or maybe a tast test that called
    // chrome.EnableStackSampledMetrics().
    return RelativePopulations{100, 0};
  }
#endif

  CHECK(*release_channel == version_info::Channel::CANARY ||
        *release_channel == version_info::Channel::DEV);

  return RelativePopulations{80, 20};
}

double DefaultPlatformConfiguration::GetChildProcessPerExecutionEnableFraction(
    metrics::CallStackProfileParams::Process process) const {
  DCHECK_NE(metrics::CallStackProfileParams::Process::kBrowser, process);

  // Profile all supported processes in browser test mode.
  if (browser_test_mode_enabled()) {
    return 1.0;
  }

  switch (process) {
    case metrics::CallStackProfileParams::Process::kGpu:
    case metrics::CallStackProfileParams::Process::kNetworkService:
      return 1.0;

    case metrics::CallStackProfileParams::Process::kRenderer:
      // Run the profiler in 20% of the processes to collect roughly as many
      // profiles for renderer processes as browser processes.
      return 0.2;

    default:
      return 0.0;
  }
}

absl::optional<metrics::CallStackProfileParams::Process>
DefaultPlatformConfiguration::ChooseEnabledProcess() const {
  // Ignore the setting, sampling more than one process.
  return absl::nullopt;
}

bool DefaultPlatformConfiguration::IsEnabledForThread(
    metrics::CallStackProfileParams::Process process,
    metrics::CallStackProfileParams::Thread thread,
    absl::optional<version_info::Channel> release_channel) const {
  // Enable for all supported threads.
  return true;
}

bool DefaultPlatformConfiguration::IsSupportedForChannel(
    absl::optional<version_info::Channel> release_channel) const {
  // The profiler is always supported for local builds and the CQ.
  if (!release_channel)
    return true;

#if BUILDFLAG(IS_CHROMEOS)
  if (browser_test_mode_enabled()) {
    // This is a browser test or maybe a tast test that called
    // chrome.EnableStackSampledMetrics().
    return true;
  }
#endif

  // Canary and dev are the only channels currently supported in release
  // builds.
  return *release_channel == version_info::Channel::CANARY ||
         *release_channel == version_info::Channel::DEV;
}

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
// The configuration to use for the Android platform. Applies to ARM32 which is
// the only Android architecture currently supported by StackSamplingProfiler.
// Defined in terms of DefaultPlatformConfiguration where Android does not
// differ from the default case.
class AndroidPlatformConfiguration : public DefaultPlatformConfiguration {
 public:
  explicit AndroidPlatformConfiguration(
      bool browser_test_mode_enabled,
      base::RepeatingCallback<bool(double)> is_enabled_on_dev_callback);

  RelativePopulations GetEnableRates(
      absl::optional<version_info::Channel> release_channel) const override;

  double GetChildProcessPerExecutionEnableFraction(
      metrics::CallStackProfileParams::Process process) const override;

  absl::optional<metrics::CallStackProfileParams::Process>
  ChooseEnabledProcess() const override;

  bool IsEnabledForThread(
      metrics::CallStackProfileParams::Process process,
      metrics::CallStackProfileParams::Thread thread,
      absl::optional<version_info::Channel> release_channel) const override;

 private:
  // Whether profiling is enabled on a thread type for Android DEV channel.
  const base::flat_map<metrics::CallStackProfileParams::Thread, bool>
      thread_enabled_on_dev_;
};

AndroidPlatformConfiguration::AndroidPlatformConfiguration(
    bool browser_test_mode_enabled,
    base::RepeatingCallback<bool(double)> is_enabled_on_dev_callback)
    : DefaultPlatformConfiguration(browser_test_mode_enabled),
      thread_enabled_on_dev_(
          base::MakeFlatMap<metrics::CallStackProfileParams::Thread, bool>(
              []() {
                std::vector<metrics::CallStackProfileParams::Thread> threads;
                for (int i = 0;
                     i <= static_cast<int>(
                              metrics::CallStackProfileParams::Thread::kMax);
                     ++i) {
                  threads.push_back(
                      static_cast<metrics::CallStackProfileParams::Thread>(i));
                }
                return threads;
              }(),
              {},
              [&](metrics::CallStackProfileParams::Thread thread) {
                // Only enable 25% of threads on Dev channel as analysis
                // shows 25% thread enable rate will give us sufficient
                // resolution (100us).
                return std::make_pair(thread,
                                      is_enabled_on_dev_callback.Run(0.25));
              })) {}

ThreadProfilerPlatformConfiguration::RelativePopulations
AndroidPlatformConfiguration::GetEnableRates(
    absl::optional<version_info::Channel> release_channel) const {
  // Always enable profiling in local/CQ builds or browser test mode.
  if (!release_channel.has_value() || browser_test_mode_enabled()) {
    return RelativePopulations{100, 0};
  }

  DCHECK(*release_channel == version_info::Channel::CANARY ||
         *release_channel == version_info::Channel::DEV);

  // For 80% of population always enable profiling.
  // For 20% of population
  // - 50% within the subgroup, i.e. 10% of total population, enable profiling.
  // - 50% within the subgroup, disable profiling.
  // This results a total of 90% enable rate.
  return RelativePopulations{80, 20};
}

double AndroidPlatformConfiguration::GetChildProcessPerExecutionEnableFraction(
    metrics::CallStackProfileParams::Process process) const {
  // Unconditionally profile child processes that match ChooseEnabledProcess().
  return 1.0;
}

absl::optional<metrics::CallStackProfileParams::Process>
AndroidPlatformConfiguration::ChooseEnabledProcess() const {
  // Weights are set such that we will receive similar amount of data from
  // each process type. The value is calculated based on Canary/Dev channel
  // data collected when all process are sampled.
  const struct {
    metrics::CallStackProfileParams::Process process;
    int weight;
  } process_enable_weights[] = {
      {metrics::CallStackProfileParams::Process::kBrowser, 50},
      {metrics::CallStackProfileParams::Process::kGpu, 40},
      {metrics::CallStackProfileParams::Process::kRenderer, 10},
  };

  int total_weight = 0;
  for (const auto& process_enable_weight : process_enable_weights) {
    total_weight += process_enable_weight.weight;
  }
  DCHECK_EQ(100, total_weight);

  int chosen = base::RandInt(0, total_weight - 1);  // Max is inclusive.
  int cumulative_weight = 0;
  for (const auto& process_enable_weight : process_enable_weights) {
    if (chosen >= cumulative_weight &&
        chosen < cumulative_weight + process_enable_weight.weight) {
      return process_enable_weight.process;
    }
    cumulative_weight += process_enable_weight.weight;
  }
  NOTREACHED();
  return absl::nullopt;
}

bool AndroidPlatformConfiguration::IsEnabledForThread(
    metrics::CallStackProfileParams::Process process,
    metrics::CallStackProfileParams::Thread thread,
    absl::optional<version_info::Channel> release_channel) const {
  if (!release_channel.has_value() || browser_test_mode_enabled()) {
    return true;
  }

  switch (*release_channel) {
    case version_info::Channel::DEV: {
      const auto entry = thread_enabled_on_dev_.find(thread);
      CHECK(entry != thread_enabled_on_dev_.end());
      return entry->second;
    }
    case version_info::Channel::CANARY:
      return true;
    default:
      return false;
  }
}

#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)

}  // namespace

// static
std::unique_ptr<ThreadProfilerPlatformConfiguration>
ThreadProfilerPlatformConfiguration::Create(
    bool browser_test_mode_enabled,
    base::RepeatingCallback<bool(double)> is_enabled_on_dev_callback) {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
  return std::make_unique<AndroidPlatformConfiguration>(
      browser_test_mode_enabled, is_enabled_on_dev_callback);
#else
  return std::make_unique<DefaultPlatformConfiguration>(
      browser_test_mode_enabled);
#endif
}

bool ThreadProfilerPlatformConfiguration::IsSupported(
    absl::optional<version_info::Channel> release_channel) const {
// `ThreadProfiler` is currently not supported on ARM64, even if
// `base::StackSamplingProfiler` may support it.
//
// TODO(crbug.com/1392158): Remove this conditional.
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)
  return false;
#else
  return base::StackSamplingProfiler::IsSupportedForCurrentPlatform() &&
         IsSupportedForChannel(release_channel);
#endif
}

// static
bool ThreadProfilerPlatformConfiguration::IsEnabled(
    double enabled_probability) {
  DCHECK_GE(enabled_probability, 0.0);
  DCHECK_LE(enabled_probability, 1.0);
  return base::RandDouble() < enabled_probability;
}
