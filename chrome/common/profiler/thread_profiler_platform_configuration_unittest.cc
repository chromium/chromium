// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include "base/profiler/profiler_buildflags.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if (defined(OS_WIN) && defined(ARCH_CPU_X86_64)) || \
    (defined(OS_MAC) && defined(ARCH_CPU_X86_64)) || \
    (defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE))
#define THREAD_PROFILER_SUPPORTED_ON_PLATFORM true
#else
#define THREAD_PROFILER_SUPPORTED_ON_PLATFORM false
#endif

// The browser_test_mode_enabled=true scenario is already covered by the browser
// tests so doesn't require separate testing here.
TEST(ThreadProfilerPlatformConfigurationTest, IsSupported) {
  const std::unique_ptr<ThreadProfilerPlatformConfiguration> config =
      ThreadProfilerPlatformConfiguration::Create(
          /*browser_test_mode_enabled=*/false);
#if !THREAD_PROFILER_SUPPORTED_ON_PLATFORM
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::UNKNOWN));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::CANARY));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::DEV));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::BETA));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::STABLE));

  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/false,
                                   version_info::Channel::UNKNOWN));
#elif defined(OS_ANDROID)
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::UNKNOWN));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::CANARY));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::DEV));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::BETA));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::STABLE));

  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/false,
                                   version_info::Channel::UNKNOWN));
#else
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::UNKNOWN));
  EXPECT_TRUE(config->IsSupported(/*is_chrome_branded=*/true,
                                  version_info::Channel::CANARY));
  EXPECT_TRUE(config->IsSupported(/*is_chrome_branded=*/true,
                                  version_info::Channel::DEV));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::BETA));
  EXPECT_FALSE(config->IsSupported(/*is_chrome_branded=*/true,
                                   version_info::Channel::STABLE));

  EXPECT_TRUE(config->IsSupported(/*is_chrome_branded=*/false,
                                  version_info::Channel::UNKNOWN));
#endif
}
