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

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

#if (defined(OS_WIN) && defined(ARCH_CPU_X86_64)) || \
    (defined(OS_MAC) && defined(ARCH_CPU_X86_64)) || \
    (defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE))
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

  EXPECT_FALSE(config()->IsSupported(base::nullopt));
#elif defined(OS_ANDROID)
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::UNKNOWN));
  EXPECT_TRUE(config()->IsSupported(version_info::Channel::CANARY));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::DEV));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::BETA));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::STABLE));

  EXPECT_FALSE(config()->IsSupported(base::nullopt));
#else
#if defined(OS_MAC)
  // Sampling profiler does not work on macOS 11.0 yet:
  // https://crbug.com/1101399
  const bool on_canary = base::mac::IsAtMostOS10_15();
  const bool on_dev = base::mac::IsAtMostOS10_15();
  const bool on_default = base::mac::IsAtMostOS10_15();
#else
  const bool on_canary = true;
  const bool on_dev = true;
  const bool on_default = true;
#endif
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::UNKNOWN));
  EXPECT_EQ(on_canary, config()->IsSupported(version_info::Channel::CANARY));
  EXPECT_EQ(on_dev, config()->IsSupported(version_info::Channel::DEV));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::BETA));
  EXPECT_FALSE(config()->IsSupported(version_info::Channel::STABLE));

  EXPECT_EQ(on_default, config()->IsSupported(base::nullopt));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             GetRuntimeModuleState) {
  using RuntimeModuleState =
      ThreadProfilerPlatformConfiguration::RuntimeModuleState;
#if defined(OS_ANDROID)
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
#if defined(OS_ANDROID)
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
            config()->GetEnableRates(base::nullopt));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             GetChildProcessEnableFraction) {
#if defined(OS_ANDROID)
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::GPU_PROCESS));
  EXPECT_EQ(0.4, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::RENDERER_PROCESS));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::NETWORK_SERVICE_PROCESS));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::UTILITY_PROCESS));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::UNKNOWN_PROCESS));
#else
  EXPECT_EQ(1.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::GPU_PROCESS));
  EXPECT_EQ(0.2, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::RENDERER_PROCESS));
  EXPECT_EQ(1.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::NETWORK_SERVICE_PROCESS));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::UTILITY_PROCESS));
  EXPECT_EQ(0.0, config()->GetChildProcessEnableFraction(
                     metrics::CallStackProfileParams::UNKNOWN_PROCESS));
#endif
}

MAYBE_PLATFORM_CONFIG_TEST_F(ThreadProfilerPlatformConfigurationTest,
                             IsEnabledForThread) {
#if defined(OS_ANDROID)
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::BROWSER_PROCESS,
      metrics::CallStackProfileParams::MAIN_THREAD));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::BROWSER_PROCESS,
      metrics::CallStackProfileParams::IO_THREAD));

  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::GPU_PROCESS,
      metrics::CallStackProfileParams::MAIN_THREAD));
  EXPECT_FALSE(
      config()->IsEnabledForThread(metrics::CallStackProfileParams::GPU_PROCESS,
                                   metrics::CallStackProfileParams::IO_THREAD));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::GPU_PROCESS,
      metrics::CallStackProfileParams::COMPOSITOR_THREAD));

  EXPECT_TRUE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::RENDERER_PROCESS,
      metrics::CallStackProfileParams::MAIN_THREAD));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::RENDERER_PROCESS,
      metrics::CallStackProfileParams::IO_THREAD));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::RENDERER_PROCESS,
      metrics::CallStackProfileParams::COMPOSITOR_THREAD));
  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::RENDERER_PROCESS,
      metrics::CallStackProfileParams::SERVICE_WORKER_THREAD));

  EXPECT_FALSE(config()->IsEnabledForThread(
      metrics::CallStackProfileParams::NETWORK_SERVICE_PROCESS,
      metrics::CallStackProfileParams::IO_THREAD));
#else
  // Profiling should be enabled without restriction across all threads. Not all
  // these combinations actually make sense or are implemented in the code, but
  // iterating over all combinations is the simplest way to test.
  for (int i = 0; i <= metrics::CallStackProfileParams::MAX_PROCESS; ++i) {
    const auto process =
        static_cast<metrics::CallStackProfileParams::Process>(i);
    for (int j = 0; j <= metrics::CallStackProfileParams::MAX_THREAD; ++j) {
      const auto thread =
          static_cast<metrics::CallStackProfileParams::Thread>(j);
      EXPECT_TRUE(config()->IsEnabledForThread(process, thread));
    }
  }
#endif
}
