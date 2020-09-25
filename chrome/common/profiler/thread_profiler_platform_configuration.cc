// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include "base/profiler/stack_sampling_profiler.h"
#include "build/build_config.h"

namespace {

// The default configuration to use in the absence of special circumstances on a
// specific platform.
class DefaultPlatformConfiguration
    : public ThreadProfilerPlatformConfiguration {
 public:
  explicit DefaultPlatformConfiguration(bool browser_test_mode_enabled);

 protected:
  bool IsSupportedForChannel(bool is_chrome_branded,
                             version_info::Channel channel) const override;

  bool browser_test_mode_enabled() const { return browser_test_mode_enabled_; }

 private:
  const bool browser_test_mode_enabled_;
};

DefaultPlatformConfiguration::DefaultPlatformConfiguration(
    bool browser_test_mode_enabled)
    : browser_test_mode_enabled_(browser_test_mode_enabled) {}

bool DefaultPlatformConfiguration::IsSupportedForChannel(
    bool is_chrome_branded,
    version_info::Channel channel) const {
  // The profiler is always supported for local builds and the CQ.
  if (!is_chrome_branded)
    return true;

  // Canary and dev are the only channels currently supported in release
  // builds.
  return channel == version_info::Channel::CANARY ||
         channel == version_info::Channel::DEV;
}

#if defined(OS_ANDROID)
// The configuration to use for the Android platform. Applies to ARM32 which is
// the only Android architecture currently supported by StackSamplingProfiler.
// Defined in terms of DefaultPlatformConfiguration where Android does not
// differ from the default case.
class AndroidPlatformConfiguration : public DefaultPlatformConfiguration {
 public:
  explicit AndroidPlatformConfiguration(bool browser_test_mode_enabled);

 protected:
  bool IsSupportedForChannel(bool is_chrome_branded,
                             version_info::Channel channel) const override;
};

AndroidPlatformConfiguration::AndroidPlatformConfiguration(
    bool browser_test_mode_enabled)
    : DefaultPlatformConfiguration(browser_test_mode_enabled) {}

bool AndroidPlatformConfiguration::IsSupportedForChannel(
    bool is_chrome_branded,
    version_info::Channel channel) const {
  // On Android profiling is only enabled in its own dedicated browser tests
  // in local builds and the CQ.
  // TODO(https://crbug.com/1004855): Enable across all browser tests.
  return browser_test_mode_enabled();
}
#endif  // defined(OS_ANDROID)

}  // namespace

// static
std::unique_ptr<ThreadProfilerPlatformConfiguration>
ThreadProfilerPlatformConfiguration::Create(bool browser_test_mode_enabled) {
#if defined(OS_ANDROID)
  using PlatformConfiguration = AndroidPlatformConfiguration;
#else
  using PlatformConfiguration = DefaultPlatformConfiguration;
#endif
  return std::make_unique<PlatformConfiguration>(browser_test_mode_enabled);
}

bool ThreadProfilerPlatformConfiguration::IsSupported(
    bool is_chrome_branded,
    version_info::Channel channel) const {
  return base::StackSamplingProfiler::IsSupportedForCurrentPlatform() &&
         IsSupportedForChannel(is_chrome_branded, channel);
}
