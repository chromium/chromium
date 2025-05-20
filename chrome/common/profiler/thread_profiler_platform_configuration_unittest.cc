// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

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
#define THREAD_PROFILER_SUPPORTED_ON_PLATFORM true
#else
#define THREAD_PROFILER_SUPPORTED_ON_PLATFORM false
#endif

#if THREAD_PROFILER_SUPPORTED_ON_PLATFORM
#define MAYBE_PLATFORM_CONFIG_TEST_F(suite, test) TEST_F(suite, test)
#else
#define MAYBE_PLATFORM_CONFIG_TEST_F(suite, test) TEST_F(suite, DISABLED_##test)
#endif

namespace {

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
std::ostream& operator<<(
    std::ostream& strm,
    const ThreadProfilerPlatformConfiguration::RelativePopulations&
        populations) {
  return strm << "{" << populations.enabled << ", " << populations.experiment
              << "}";
}

bool operator==(
    const ThreadProfilerPlatformConfiguration::RelativePopulations& a,
    const ThreadProfilerPlatformConfiguration::RelativePopulations& b) {
  return a.enabled == b.enabled && a.experiment == b.experiment;
}

TEST_F(ThreadProfilerPlatformConfigurationTest, IsSupported) {
#if !THREAD_PROFILER_SUPPORTED_ON_PLATFORM
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::UNKNOWN));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::CANARY));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::DEV));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::BETA));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::STABLE));

  EXPECT_FALSE(config()->IsSupported(std::nullopt));
#else
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::UNKNOWN));
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::CANARY));
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::DEV));
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::BETA));
#if BUILDFLAG(IS_ANDROID)
#if defined(ARCH_CPU_ARM64)
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::STABLE));
#else   // defined(ARCH_CPU_ARM64)
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::STABLE));
#endif  // defined(ARCH_CPU_ARM64)
#else   // BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::STABLE));
#endif  // BUILDFLAG(IS_ANDROID)

  EXPECT_TRUE(config()->IsSupported(std::nullopt));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             GetEnableRates) {
  using RelativePopulations =
      ThreadProfilerPlatformConfiguration::RelativePopulations;
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ((RelativePopulations{0.0, 0.0, 100.0}),
            config()->GetEnableRates(version_info::Channel::CANARY));
  EXPECT_EQ((RelativePopulations{0.0, 0.0, 100.0}),
            config()->GetEnableRates(version_info::Channel::DEV));
  EXPECT_EQ((RelativePopulations{0.0, 0.0, 100.0}),
            config()->GetEnableRates(version_info::Channel::BETA));
#if defined(ARCH_CPU_ARM64)
  EXPECT_EQ((RelativePopulations{100.0 - 0.0001, 0.0, 0.0001}),
            config()->GetEnableRates(version_info::Channel::STABLE));
#else
  EXPECT_EQ((RelativePopulations{100.0, 0.0, 0.0}),
            config()->GetEnableRates(version_info::Channel::STABLE));
#endif
  // Note: death tests aren't supported on Android. Otherwise this test would
  // check that the other inputs result in CHECKs.
#else
  EXPECT_CHECK_DEATH(config()->GetEnableRates(version_info::Channel::UNKNOWN));
  EXPECT_EQ((RelativePopulations{0.0, 80.0, 20.0}),
            config()->GetEnableRates(version_info::Channel::CANARY));
  EXPECT_EQ((RelativePopulations{0.0, 80.0, 20.0}),
            config()->GetEnableRates(version_info::Channel::DEV));
  EXPECT_EQ((RelativePopulations{90.0, 0.0, 10.0}),
            config()->GetEnableRates(version_info::Channel::BETA));
  EXPECT_EQ((RelativePopulations{100.0 - 0.006, 0.0, 0.006}),
            config()->GetEnableRates(version_info::Channel::STABLE));

  EXPECT_EQ((RelativePopulations{0.0, 100.0, 0.0}),
            config()->GetEnableRates(std::nullopt));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             GetChildProcessPerExecutionEnableFraction) {
  EXPECT_EQ(1.0, config()->GetChildProcessPerExecutionEnableFraction(
                     sampling_profiler::ProfilerProcessType::kGpu));
  EXPECT_EQ(1.0, config()->GetChildProcessPerExecutionEnableFraction(
                     sampling_profiler::ProfilerProcessType::kNetworkService));

#if BUILDFLAG(IS_ANDROID)
  // Android child processes that match ChooseEnabledProcess() should be
  // profiled unconditionally.
  EXPECT_EQ(1.0, config()->GetChildProcessPerExecutionEnableFraction(
                     sampling_profiler::ProfilerProcessType::kRenderer));
  EXPECT_EQ(1.0, config()->GetChildProcessPerExecutionEnableFraction(
                     sampling_profiler::ProfilerProcessType::kUnknown));
#else
  EXPECT_EQ(0.2, config()->GetChildProcessPerExecutionEnableFraction(
                     sampling_profiler::ProfilerProcessType::kRenderer));
  EXPECT_EQ(0.0, config()->GetChildProcessPerExecutionEnableFraction(
                     sampling_profiler::ProfilerProcessType::kUnknown));
#endif
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
