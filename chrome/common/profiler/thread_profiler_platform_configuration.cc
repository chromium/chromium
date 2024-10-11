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
#include "components/sampling_profiler/process_type.h"

namespace {

// The default configuration to use in the absence of special circumstances on a
// specific platform.
class DefaultPlatformConfiguration
    : public ThreadProfilerPlatformConfiguration {
 public:
  explicit DefaultPlatformConfiguration(bool browser_test_mode_enabled);

  RelativePopulations GetEnableRates(
      std::optional<version_info::Channel> release_channel) const override;

  double GetChildProcessPerExecutionEnableFraction(
      sampling_profiler::ProfilerProcessType process) const override;

  std::optional<sampling_profiler::ProfilerProcessType> ChooseEnabledProcess()
      const override;

  bool IsEnabledForThread(
      sampling_profiler::ProfilerProcessType process,
      sampling_profiler::ProfilerThreadType thread,
      std::optional<version_info::Channel> release_channel) const override;

 protected:
  bool IsSupportedForChannel(
      std::optional<version_info::Channel> release_channel) const override;

  bool browser_test_mode_enabled() const { return browser_test_mode_enabled_; }

 private:
  const bool browser_test_mode_enabled_;
};

DefaultPlatformConfiguration::DefaultPlatformConfiguration(
    bool browser_test_mode_enabled)
    : browser_test_mode_enabled_(browser_test_mode_enabled) {}

ThreadProfilerPlatformConfiguration::RelativePopulations
DefaultPlatformConfiguration::GetEnableRates(
    std::optional<version_info::Channel> release_channel) const {
  CHECK(IsSupportedForChannel(release_channel));

  if (!release_channel) {
    // This is a local/CQ build.
    return RelativePopulations{0, 100, 0};
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (browser_test_mode_enabled()) {
    // This is a browser test or maybe a tast test that called
    // chrome.EnableStackSampledMetrics().
    return RelativePopulations{0, 100, 0};
  }
#endif

  CHECK(*release_channel == version_info::Channel::CANARY ||
        *release_channel == version_info::Channel::DEV ||
        *release_channel == version_info::Channel::BETA);

  if (*release_channel == version_info::Channel::BETA) {
    // TODO(crbug.com/1497983): Ramp up enable rate on Non-Android platforms.
    return RelativePopulations{90, 0, 10};
  }
  return RelativePopulations{0, 80, 20};
}

double DefaultPlatformConfiguration::GetChildProcessPerExecutionEnableFraction(
    sampling_profiler::ProfilerProcessType process) const {
  DCHECK_NE(sampling_profiler::ProfilerProcessType::kBrowser, process);

  // Profile all supported processes in browser test mode.
  if (browser_test_mode_enabled()) {
    return 1.0;
  }

  switch (process) {
    case sampling_profiler::ProfilerProcessType::kGpu:
    case sampling_profiler::ProfilerProcessType::kNetworkService:
      return 1.0;

    case sampling_profiler::ProfilerProcessType::kRenderer:
      // Run the profiler in 20% of the processes to collect roughly as many
      // profiles for renderer processes as browser processes.
      return 0.2;

    default:
      return 0.0;
  }
}

std::optional<sampling_profiler::ProfilerProcessType>
DefaultPlatformConfiguration::ChooseEnabledProcess() const {
  // Ignore the setting, sampling more than one process.
  return std::nullopt;
}

bool DefaultPlatformConfiguration::IsEnabledForThread(
    sampling_profiler::ProfilerProcessType process,
    sampling_profiler::ProfilerThreadType thread,
    std::optional<version_info::Channel> release_channel) const {
  // Enable for all supported threads.
  return true;
}

bool DefaultPlatformConfiguration::IsSupportedForChannel(
    std::optional<version_info::Channel> release_channel) const {
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

  // Canary, dev, and beta are the only channels currently supported in release
  // builds.
  return *release_channel == version_info::Channel::CANARY ||
         *release_channel == version_info::Channel::DEV ||
         *release_channel == version_info::Channel::BETA;
}

#if BUILDFLAG(IS_ANDROID)
// The configuration to use for the Android platform. Defined in terms of
// DefaultPlatformConfiguration where Android does not differ from the default
// case.
class AndroidPlatformConfiguration : public DefaultPlatformConfiguration {
 public:
  explicit AndroidPlatformConfiguration(
      bool browser_test_mode_enabled,
      base::RepeatingCallback<bool(double)> is_enabled_on_dev_callback);

  RelativePopulations GetEnableRates(
      std::optional<version_info::Channel> release_channel) const override;

  double GetChildProcessPerExecutionEnableFraction(
      sampling_profiler::ProfilerProcessType process) const override;

  std::optional<sampling_profiler::ProfilerProcessType> ChooseEnabledProcess()
      const override;

  bool IsEnabledForThread(
      sampling_profiler::ProfilerProcessType process,
      sampling_profiler::ProfilerThreadType thread,
      std::optional<version_info::Channel> release_channel) const override;

 private:
  // Whether profiling is enabled on a thread type for Android DEV channel.
  const base::flat_map<sampling_profiler::ProfilerThreadType, bool>
      thread_enabled_on_dev_;
};

AndroidPlatformConfiguration::AndroidPlatformConfiguration(
    bool browser_test_mode_enabled,
    base::RepeatingCallback<bool(double)> is_enabled_on_dev_callback)
    : DefaultPlatformConfiguration(browser_test_mode_enabled),
      thread_enabled_on_dev_(
          base::MakeFlatMap<sampling_profiler::ProfilerThreadType, bool>(
              []() {
                std::vector<sampling_profiler::ProfilerThreadType> threads;
                for (int i = 0;
                     i <= static_cast<int>(
                              sampling_profiler::ProfilerThreadType::kMax);
                     i++) {
                  threads.push_back(
                      static_cast<sampling_profiler::ProfilerThreadType>(i));
                }
                return threads;
              }(),
              {},
              [&](sampling_profiler::ProfilerThreadType thread) {
                // Only enable 25% of threads on Dev channel as analysis
                // shows 25% thread enable rate will give us sufficient
                // resolution (100us).
                return std::make_pair(thread,
                                      is_enabled_on_dev_callback.Run(0.25));
              })) {}

ThreadProfilerPlatformConfiguration::RelativePopulations
AndroidPlatformConfiguration::GetEnableRates(
    std::optional<version_info::Channel> release_channel) const {
  // Always enable profiling in local/CQ builds or browser test mode.
  if (!release_channel.has_value() || browser_test_mode_enabled()) {
    return RelativePopulations{0, 100, 0};
  }

  CHECK(*release_channel == version_info::Channel::CANARY ||
        *release_channel == version_info::Channel::DEV ||
        *release_channel == version_info::Channel::BETA);

  if (*release_channel == version_info::Channel::BETA) {
    // For 100% of population
    // - 1/2 within the subgroup, i.e. 50% of total population, enable
    // profiling.
    // - 1/2 within the subgroup, disable profiling.
    // This results a total of 50% enable rate.
    return RelativePopulations{0, 0, 100};
  }

  // For 100% of population
  // - 1/2 within the subgroup, i.e. 50% of total population, enable profiling.
  // - 1/2 within the subgroup, disable profiling.
  // This results a total of 50% enable rate.
  return RelativePopulations{0, 0, 100};
}

double AndroidPlatformConfiguration::GetChildProcessPerExecutionEnableFraction(
    sampling_profiler::ProfilerProcessType process) const {
  // Unconditionally profile child processes that match ChooseEnabledProcess().
  return 1.0;
}

std::optional<sampling_profiler::ProfilerProcessType>
AndroidPlatformConfiguration::ChooseEnabledProcess() const {
  // Weights are set such that we will receive similar amount of data from
  // each process type. The value is calculated based on Canary/Dev channel
  // data collected when all process are sampled.
  const struct {
    sampling_profiler::ProfilerProcessType process;
    int weight;
  } process_enable_weights[] = {
      {sampling_profiler::ProfilerProcessType::kBrowser, 50},
      {sampling_profiler::ProfilerProcessType::kGpu, 40},
      {sampling_profiler::ProfilerProcessType::kRenderer, 10},
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
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

bool AndroidPlatformConfiguration::IsEnabledForThread(
    sampling_profiler::ProfilerProcessType process,
    sampling_profiler::ProfilerThreadType thread,
    std::optional<version_info::Channel> release_channel) const {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)
  // For now, we only enable SSM in the Browser process and Main thread on
  // Android 64, since Libunwindstack doesn't support JavaScript.
  if (!(process == sampling_profiler::ProfilerProcessType::kBrowser &&
        thread == sampling_profiler::ProfilerThreadType::kMain)) {
    return false;
  }
#endif
  if (!release_channel.has_value() || browser_test_mode_enabled()) {
    return true;
  }

  switch (*release_channel) {
    // TODO(crbug.com/40287243): Adjust thread-level enable rate for beta
    // channel based on the data volume after launch. Temporarily use the same
    // thread-level enable rate as dev channel.
    case version_info::Channel::BETA:
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

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

// static
std::unique_ptr<ThreadProfilerPlatformConfiguration>
ThreadProfilerPlatformConfiguration::Create(
    bool browser_test_mode_enabled,
    base::RepeatingCallback<bool(double)> is_enabled_on_dev_callback) {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<AndroidPlatformConfiguration>(
      browser_test_mode_enabled, is_enabled_on_dev_callback);
#else
  return std::make_unique<DefaultPlatformConfiguration>(
      browser_test_mode_enabled);
#endif
}

bool ThreadProfilerPlatformConfiguration::IsSupported(
    std::optional<version_info::Channel> release_channel) const {
  return base::StackSamplingProfiler::IsSupportedForCurrentPlatform() &&
         IsSupportedForChannel(release_channel);
}

// static
bool ThreadProfilerPlatformConfiguration::IsEnabled(
    double enabled_probability) {
  DCHECK_GE(enabled_probability, 0.0);
  DCHECK_LE(enabled_probability, 1.0);
  return base::RandDouble() < enabled_probability;
}
