// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include <utility>

#include "base/profiler/profiler_buildflags.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
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
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::STABLE));

  EXPECT_TRUE(config()->IsSupported(std::nullopt));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             GetEnableRates) {
  using RelativePopulations =
      ThreadProfilerPlatformConfiguration::RelativePopulations;
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ((RelativePopulations{0, 0, 100}),
            config()->GetEnableRates(version_info::Channel::CANARY));
  EXPECT_EQ((RelativePopulations{0, 0, 100}),
            config()->GetEnableRates(version_info::Channel::DEV));
  EXPECT_EQ((RelativePopulations{0, 0, 100}),
            config()->GetEnableRates(version_info::Channel::BETA));
  // Note: death tests aren't supported on Android. Otherwise this test would
  // check that the other inputs result in CHECKs.
#else
  EXPECT_CHECK_DEATH(config()->GetEnableRates(version_info::Channel::UNKNOWN));
  EXPECT_EQ((RelativePopulations{0, 80, 20}),
            config()->GetEnableRates(version_info::Channel::CANARY));
  EXPECT_EQ((RelativePopulations{0, 80, 20}),
            config()->GetEnableRates(version_info::Channel::DEV));
  EXPECT_EQ((RelativePopulations{90, 0, 10}),
            config()->GetEnableRates(version_info::Channel::BETA));
  EXPECT_CHECK_DEATH(config()->GetEnableRates(version_info::Channel::STABLE));

  EXPECT_EQ((RelativePopulations{0, 100, 0}),
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

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             IsEnabledForThread) {
  // Profiling should be enabled without restriction across all threads,
  // assuming it is enabled for corresponding process. Not all these
  // combinations actually make sense or are implemented in the code, but
  // iterating over all combinations is the simplest way to test.
  for (int i = 0;
       i <= static_cast<int>(sampling_profiler::ProfilerProcessType::kMax);
       ++i) {
    const auto process = static_cast<sampling_profiler::ProfilerProcessType>(i);
    for (int j = 0;
         j <= static_cast<int>(sampling_profiler::ProfilerThreadType::kMax);
         ++j) {
      const auto thread = static_cast<sampling_profiler::ProfilerThreadType>(j);

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)
      if (process == sampling_profiler::ProfilerProcessType::kBrowser &&
          thread == sampling_profiler::ProfilerThreadType::kMain) {
        EXPECT_TRUE(config()->IsEnabledForThread(
            process, thread, version_info::Channel::CANARY));
      } else {
        EXPECT_FALSE(config()->IsEnabledForThread(
            process, thread, version_info::Channel::CANARY));
      }
#else
      EXPECT_TRUE(config()->IsEnabledForThread(process, thread,
                                               version_info::Channel::CANARY));
#endif

#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL)
      auto android_config1 = ThreadProfilerPlatformConfiguration::Create(
          /* browser_test_mode_enabled=*/false,
          base::BindLambdaForTesting([](double probability) { return true; }));
      EXPECT_TRUE(android_config1->IsEnabledForThread(
          process, thread, version_info::Channel::DEV));
      auto android_config2 = ThreadProfilerPlatformConfiguration::Create(
          /* browser_test_mode_enabled=*/false,
          base::BindLambdaForTesting([](double probability) { return false; }));
      EXPECT_FALSE(android_config2->IsEnabledForThread(
          process, thread, version_info::Channel::DEV));
#elif BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARM64)
      auto android_config1 = ThreadProfilerPlatformConfiguration::Create(
          /* browser_test_mode_enabled=*/false,
          base::BindLambdaForTesting([](double probability) { return true; }));
      auto android_config2 = ThreadProfilerPlatformConfiguration::Create(
          /* browser_test_mode_enabled=*/false,
          base::BindLambdaForTesting([](double probability) { return false; }));

      if (process == sampling_profiler::ProfilerProcessType::kBrowser &&
          thread == sampling_profiler::ProfilerThreadType::kMain) {
        EXPECT_TRUE(android_config1->IsEnabledForThread(
            process, thread, version_info::Channel::DEV));
      } else {
        EXPECT_FALSE(android_config1->IsEnabledForThread(
            process, thread, version_info::Channel::DEV));
      }

      EXPECT_FALSE(android_config2->IsEnabledForThread(
          process, thread, version_info::Channel::DEV));
#else
      EXPECT_TRUE(config()->IsEnabledForThread(process, thread,
                                               version_info::Channel::DEV));
#endif
    }
  }
}
