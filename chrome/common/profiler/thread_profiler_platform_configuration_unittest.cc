// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include <utility>

#include "base/profiler/profiler_buildflags.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE))
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

  EXPECT_FALSE(config()->IsSupported(absl::nullopt));
#elif BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::UNKNOWN));
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::CANARY));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::DEV));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::BETA));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::STABLE));

  EXPECT_FALSE(config()->IsSupported(absl::nullopt));
#else
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::UNKNOWN));
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::CANARY));
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::DEV));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::BETA));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::STABLE));

  EXPECT_TRUE(config()->IsSupported(absl::nullopt));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             GetRuntimeModuleState) {
  using RuntimeModuleState =
      ThreadProfilerPlatformConfiguration::RuntimeModuleState;
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(RuntimeModuleState::kModuleNotAvailable,
            config()->GetRuntimeModuleState(version_info::Channel::UNKNOWN));
  EXPECT_EQ(RuntimeModuleState::kModuleAbsentButAvailable,
            config()->GetRuntimeModuleState(version_info::Channel::CANARY));
  EXPECT_EQ(RuntimeModuleState::kModuleAbsentButAvailable,
            config()->GetRuntimeModuleState(version_info::Channel::DEV));
  EXPECT_EQ(RuntimeModuleState::kModuleNotAvailable,
            config()->GetRuntimeModuleState(version_info::Channel::BETA));
  EXPECT_EQ(RuntimeModuleState::kModuleNotAvailable,
            config()->GetRuntimeModuleState(version_info::Channel::STABLE));

  EXPECT_EQ(RuntimeModuleState::kModuleNotAvailable,
            config()->GetRuntimeModuleState(version_info::Channel::UNKNOWN));
#else
  EXPECT_EQ(RuntimeModuleState::kModuleNotRequired,
            config()->GetRuntimeModuleState(version_info::Channel::UNKNOWN));
  EXPECT_EQ(RuntimeModuleState::kModuleNotRequired,
            config()->GetRuntimeModuleState(version_info::Channel::CANARY));
  EXPECT_EQ(RuntimeModuleState::kModuleNotRequired,
            config()->GetRuntimeModuleState(version_info::Channel::DEV));
  EXPECT_EQ(RuntimeModuleState::kModuleNotRequired,
            config()->GetRuntimeModuleState(version_info::Channel::BETA));
  EXPECT_EQ(RuntimeModuleState::kModuleNotRequired,
            config()->GetRuntimeModuleState(version_info::Channel::STABLE));

  EXPECT_EQ(RuntimeModuleState::kModuleNotRequired,
            config()->GetRuntimeModuleState(version_info::Channel::UNKNOWN));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             GetEnableRates) {
  using RelativePopulations =
      ThreadProfilerPlatformConfiguration::RelativePopulations;
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ((RelativePopulations{0, 50}),
            config()->GetEnableRates(version_info::Channel::CANARY));
  // Note: death tests aren't supported on Android. Otherwise this test would
  // check that the other inputs result in CHECKs.
#else
  EXPECT_CHECK_DEATH(config()->GetEnableRates(version_info::Channel::UNKNOWN));
  EXPECT_EQ((RelativePopulations{80, 20}),
            config()->GetEnableRates(version_info::Channel::CANARY));
  EXPECT_EQ((RelativePopulations{80, 20}),
            config()->GetEnableRates(version_info::Channel::DEV));
  EXPECT_CHECK_DEATH(config()->GetEnableRates(version_info::Channel::BETA));
  EXPECT_CHECK_DEATH(config()->GetEnableRates(version_info::Channel::STABLE));

  EXPECT_EQ((RelativePopulations{100, 0}),
            config()->GetEnableRates(absl::nullopt));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             GetChildProcessEnableFraction) {
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::Process::kGpu));
  EXPECT_EQ(0.4, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::Process::kRenderer));
  EXPECT_EQ(0.0,
            config()->GetChildProcessEnableFraction(
                metrics::CallStackProfileParams::Process::kNetworkService));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::Process::kUtility));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::Process::kUnknown));
#else
  EXPECT_EQ(1.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::Process::kGpu));
  EXPECT_EQ(0.2, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::Process::kRenderer));
  EXPECT_EQ(1.0,
            config()->GetChildProcessEnableFraction(
                metrics::CallStackProfileParams::Process::kNetworkService));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::Process::kUtility));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::Process::kUnknown));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             IsEnabledForThread) {
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kBrowser,
      metrics::CallStackProfileParams::Thread::kMain));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kBrowser,
      metrics::CallStackProfileParams::Thread::kIo));

  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kGpu,
      metrics::CallStackProfileParams::Thread::kMain));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kGpu,
      metrics::CallStackProfileParams::Thread::kIo));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kGpu,
      metrics::CallStackProfileParams::Thread::kCompositor));

  EXPECT_TRUE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kRenderer,
      metrics::CallStackProfileParams::Thread::kMain));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kRenderer,
      metrics::CallStackProfileParams::Thread::kIo));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kRenderer,
      metrics::CallStackProfileParams::Thread::kCompositor));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kRenderer,
      metrics::CallStackProfileParams::Thread::kServiceWorker));

  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::Process::kNetworkService,
      metrics::CallStackProfileParams::Thread::kIo));
#else
  // Profiling should be enabled without restriction across all threads. Not all
  // these combinations actually make sense or are implemented in the code, but
  // iterating over all combinations is the simplest way to test.
  for (int i = 0;
       i <= static_cast<int>(metrics::CallStackProfileParams::Process::kMax);
       ++i) {
    const auto process =
        static_cast<metrics::CallStackProfileParams::Process>(i);
    for (int j = 0;
         j <= static_cast<int>(metrics::CallStackProfileParams::Thread::kMax);
         ++j) {
      const auto thread =
          static_cast<metrics::CallStackProfileParams::Thread>(j);
      EXPECT_TRUE(config()->IsEnabledForThread(process, thread));
    }
  }
#endif
}
