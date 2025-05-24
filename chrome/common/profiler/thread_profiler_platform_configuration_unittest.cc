// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include <ostream>
#include <tuple>
#include <utility>

#include "base/containers/enum_set.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/sampling_profiler/process_type.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)) ||           \
    (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)) ||                   \
    (BUILDFLAG(IS_CHROMEOS) &&                                              \
     (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)))
constexpr bool kThreadProfilerSupportedOnPlatform = true;
#else
constexpr bool kThreadProfilerSupportedOnPlatform = false;
#endif

namespace {

using RelativePopulations =
    ThreadProfilerPlatformConfiguration::RelativePopulations;

class ThreadProfilerPlatformConfigurationTest : public ::testing::Test {
 public:
  // The browser_test_mode_enabled=true scenario is already covered by the
  // browser tests so doesn't require separate testing here.
  ThreadProfilerPlatformConfigurationTest()
      : config_(ThreadProfilerPlatformConfiguration::Create(
            /*browser_test_mode_enabled=*/false)) {}

  const std::unique_ptr<ThreadProfilerPlatformConfiguration>& config() {
    return config_;
  }

 private:
  const std::unique_ptr<ThreadProfilerPlatformConfiguration> config_;
};

using ThreadProfilerPlatformConfigurationDeathTest =
    ThreadProfilerPlatformConfigurationTest;

class ThreadProfilerPlatformConfigurationThreadTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  ThreadProfilerPlatformConfigurationThreadTest() {
    std::tie(enable_worker_threads_, enable_on_dev_channel_) = GetParam();
    scoped_feature_list_.InitWithFeatureState(kSamplingProfilerOnWorkerThreads,
                                              enable_worker_threads_);
    config_ = ThreadProfilerPlatformConfiguration::Create(
        /*browser_test_mode_enabled=*/false,
        /*is_enabled_on_dev_callback=*/base::BindLambdaForTesting(
            [this](double probability) { return enable_on_dev_channel_; }));
  }

  bool enable_worker_threads() const { return enable_worker_threads_; }
  bool enable_on_dev_channel() const { return enable_on_dev_channel_; }

  ThreadProfilerPlatformConfiguration* config() const { return config_.get(); }

 private:
  bool enable_worker_threads_;
  bool enable_on_dev_channel_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ThreadProfilerPlatformConfiguration> config_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ThreadProfilerPlatformConfigurationThreadTest,
    ::testing::Combine(
        ::testing::Bool(),
#if BUILDFLAG(IS_ANDROID)
        // AndroidPlatformConfiguration::IsEnabledForThread() checks the
        // channel, so test with dev channel both enabled and disabled.
        ::testing::Bool()
#else
        // DefaultPlatformConfiguration::IsEnabledForThread() doesn't check the
        // channel, so no need to test with dev channel disabled.
        ::testing::Values(true)
#endif
            ));

}  // namespace

// Glue functions to make RelativePopulations work with googletest.
std::ostream& operator<<(std::ostream& strm,
                         const RelativePopulations& populations) {
  return strm << "{" << populations.enabled << ", " << populations.experiment
              << "}";
}

bool operator==(const RelativePopulations& a, const RelativePopulations& b) {
  return a.enabled == b.enabled && a.experiment == b.experiment;
}

TEST_F(ThreadProfilerPlatformConfigurationTest, IsSupported) {
#if BUILDFLAG(IS_ANDROID) && !defined(ARCH_CPU_ARM64)
  constexpr bool kIsSupportedOnStable = false;
#else
  constexpr bool kIsSupportedOnStable = true;
#endif
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::UNKNOWN));
  EXPECT_EQ(config()->IsSupported(version_info::Channel::CANARY),
            kThreadProfilerSupportedOnPlatform);
  EXPECT_EQ(config()->IsSupported(version_info::Channel::DEV),
            kThreadProfilerSupportedOnPlatform);
  EXPECT_EQ(config()->IsSupported(version_info::Channel::BETA),
            kThreadProfilerSupportedOnPlatform);
  EXPECT_EQ(config()->IsSupported(version_info::Channel::STABLE),
            kThreadProfilerSupportedOnPlatform && kIsSupportedOnStable);
  EXPECT_EQ(config()->IsSupported(std::nullopt),
            kThreadProfilerSupportedOnPlatform);
}

TEST_F(ThreadProfilerPlatformConfigurationDeathTest, GetEnableRates) {
#if BUILDFLAG(IS_ANDROID)
  constexpr double kCanaryDevExperimentRate = 100.0;
  constexpr double kBetaExperimentRate = 100.0;
#if defined(ARCH_CPU_ARM64)
  constexpr double kStableExperimentRate = 0.0001;
#else
  constexpr double kStableExperimentRate = 0.0;
#endif
#else   // !BUILDFLAG(IS_ANDROID)
  constexpr double kCanaryDevExperimentRate = 20.0;
  constexpr double kBetaExperimentRate = 10.0;
  constexpr double kStableExperimentRate = 0.006;
#endif  // !BUILDFLAG(IS_ANDROID)

  EXPECT_CHECK_DEATH(config()->GetEnableRates(version_info::Channel::UNKNOWN));

  // 100% enabled on Canary/Dev, except for experiments.
  EXPECT_EQ((RelativePopulations{.disabled = 0.0,
                                 .enabled = 100.0 - kCanaryDevExperimentRate,
                                 .experiment = kCanaryDevExperimentRate}),
            config()->GetEnableRates(version_info::Channel::CANARY));
  EXPECT_EQ((RelativePopulations{.disabled = 0.0,
                                 .enabled = 100.0 - kCanaryDevExperimentRate,
                                 .experiment = kCanaryDevExperimentRate}),
            config()->GetEnableRates(version_info::Channel::DEV));
  // 100% disabled on Beta/Stable, except for experiments.
  EXPECT_EQ((RelativePopulations{.disabled = 100.0 - kBetaExperimentRate,
                                 .enabled = 0.0,
                                 .experiment = kBetaExperimentRate}),
            config()->GetEnableRates(version_info::Channel::BETA));
  EXPECT_EQ((RelativePopulations{.disabled = 100.0 - kStableExperimentRate,
                                 .enabled = 0.0,
                                 .experiment = kStableExperimentRate}),
            config()->GetEnableRates(version_info::Channel::STABLE));

  EXPECT_EQ((RelativePopulations{
                .disabled = 0.0, .enabled = 100.0, .experiment = 0.0}),
            config()->GetEnableRates(std::nullopt));
}

TEST_F(ThreadProfilerPlatformConfigurationTest,
       GetChildProcessPerExecutionEnableFraction) {
#if BUILDFLAG(IS_ANDROID)
  // Android child processes that match ChooseEnabledProcess() should be
  // profiled unconditionally.
  constexpr bool kAlwaysEnable = true;
#else
  constexpr bool kAlwaysEnable = false;
#endif

  EXPECT_EQ(1.0, config()->GetChildProcessPerExecutionEnableFraction(
                     sampling_profiler::ProfilerProcessType::kGpu));
  EXPECT_EQ(1.0, config()->GetChildProcessPerExecutionEnableFraction(
                     sampling_profiler::ProfilerProcessType::kNetworkService));
  EXPECT_EQ(kAlwaysEnable ? 1.0 : 0.2,
            config()->GetChildProcessPerExecutionEnableFraction(
                sampling_profiler::ProfilerProcessType::kRenderer));
  EXPECT_EQ(kAlwaysEnable ? 1.0 : 0.0,
            config()->GetChildProcessPerExecutionEnableFraction(
                sampling_profiler::ProfilerProcessType::kUnknown));
}

TEST_P(ThreadProfilerPlatformConfigurationThreadTest, IsEnabledForThread) {
  // Profiling should be enabled without restriction across all threads,
  // assuming it is enabled for corresponding process. Not all these
  // combinations actually make sense or are implemented in the code, but
  // iterating over all combinations is the simplest way to test.
  using ProcessTypes =
      base::EnumSet<sampling_profiler::ProfilerProcessType,
                    sampling_profiler::ProfilerProcessType::kUnknown,
                    sampling_profiler::ProfilerProcessType::kMax>;
  using ThreadTypes =
      base::EnumSet<sampling_profiler::ProfilerThreadType,
                    sampling_profiler::ProfilerThreadType::kUnknown,
                    sampling_profiler::ProfilerThreadType::kMax>;
  for (const auto process : ProcessTypes::All()) {
    for (const auto thread : ThreadTypes::All()) {
      SCOPED_TRACE(::testing::Message()
                   << "process type " << static_cast<int>(process)
                   << ", thread type " << static_cast<int>(thread));
      bool thread_type_enabled =
          thread == sampling_profiler::ProfilerThreadType::kThreadPoolWorker
              ? enable_worker_threads()
              : true;
      EXPECT_EQ(config()->IsEnabledForThread(process, thread,
                                             version_info::Channel::CANARY),
                thread_type_enabled);
#if !BUILDFLAG(IS_ANDROID)
      // Dev channel only has special handling on Android.
      ASSERT_TRUE(enable_on_dev_channel());
#endif
      EXPECT_EQ(config()->IsEnabledForThread(process, thread,
                                             version_info::Channel::DEV),
                thread_type_enabled && enable_on_dev_channel());
    }
  }
}
