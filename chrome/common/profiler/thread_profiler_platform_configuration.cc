// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include "base/command_line.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if defined(OS_ANDROID)
#include "chrome/android/modules/stack_unwinder/public/module.h"
#endif

namespace {

// The default configuration to use in the absence of special circumstances on a
// specific platform.
class DefaultPlatformConfiguration
    : public ThreadProfilerPlatformConfiguration {
 public:
  explicit DefaultPlatformConfiguration(bool browser_test_mode_enabled);

  // ThreadProfilerPlatformConfiguration:
  RuntimeModuleState GetRuntimeModuleState(
      bool is_chrome_branded,
      version_info::Channel channel) const override;

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

ThreadProfilerPlatformConfiguration::RuntimeModuleState
DefaultPlatformConfiguration::GetRuntimeModuleState(
    bool is_chrome_branded,
    version_info::Channel channel) const {
  return RuntimeModuleState::kModuleNotRequired;
}

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
bool IsBrowserProcess() {
  return base::CommandLine::ForCurrentProcess()
      ->GetSwitchValueASCII(switches::kProcessType)
      .empty();
}

// The configuration to use for the Android platform. Applies to ARM32 which is
// the only Android architecture currently supported by StackSamplingProfiler.
// Defined in terms of DefaultPlatformConfiguration where Android does not
// differ from the default case.
class AndroidPlatformConfiguration : public DefaultPlatformConfiguration {
 public:
  explicit AndroidPlatformConfiguration(bool browser_test_mode_enabled);

  // DefaultPlatformConfiguration:
  RuntimeModuleState GetRuntimeModuleState(
      bool is_chrome_branded,
      version_info::Channel channel) const override;

  void RequestRuntimeModuleInstall() const override;

 protected:
  bool IsSupportedForChannel(bool is_chrome_branded,
                             version_info::Channel channel) const override;
};

AndroidPlatformConfiguration::AndroidPlatformConfiguration(
    bool browser_test_mode_enabled)
    : DefaultPlatformConfiguration(browser_test_mode_enabled) {}

ThreadProfilerPlatformConfiguration::RuntimeModuleState
AndroidPlatformConfiguration::GetRuntimeModuleState(
    bool is_chrome_branded,
    version_info::Channel channel) const {
  // The module will be present in releases due to having been installed via
  // RequestRuntimeModuleInstall(), and in local/CQ builds of bundle targets
  // where the module was installed with the bundle.
  if (stack_unwinder::Module::IsInstalled())
    return RuntimeModuleState::kModulePresent;

  if (is_chrome_branded) {
    // We only want to incur the cost of universally downloading the module in
    // early channels, where profiling will occur over substantially all of
    // the population. When supporting later channels in the future we will
    // enable profiling for only a fraction of users and only download for
    // those users.
    if (channel == version_info::Channel::CANARY ||
        channel == version_info::Channel::DEV) {
      return RuntimeModuleState::kModuleAbsentButAvailable;
    }

    return RuntimeModuleState::kModuleNotAvailable;
  }

  // This is a local or CQ build of a bundle where the module was not
  // installed with the bundle, or an apk where the module is not included.
  // The module is installable from the Play Store only for released Chrome so
  // is not available in this build.
  return RuntimeModuleState::kModuleNotAvailable;
}

void AndroidPlatformConfiguration::RequestRuntimeModuleInstall() const {
  // The install can only be done in the browser process.
  CHECK(IsBrowserProcess());

  // The install occurs asynchronously, with the module available at the first
  // run of Chrome following install.
  stack_unwinder::Module::RequestInstallation();
}

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
