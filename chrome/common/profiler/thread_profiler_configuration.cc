// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_configuration.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_platform_configuration.h"
#include "components/version_info/version_info.h"

namespace {

base::LazyInstance<ThreadProfilerConfiguration>::Leaky g_configuration =
    LAZY_INSTANCE_INITIALIZER;

// Returns true if the current execution is taking place in the browser process.
// Allows the profiler to be run in a special browser test mode for testing that
// profiles are collected as expected, by providing a switch value. The test
// mode reduces the profiling duration to ensure the startup profiles complete
// well within the test timeout, and always profiles renderer processes.
bool IsBrowserTestModeEnabled() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->GetSwitchValueASCII(switches::kStartStackProfiler) ==
         switches::kStartStackProfilerBrowserTest;
}

// Returns the channel if this is a Chrome release, otherwise returns nullopt. A
// build is considered to be a Chrome release if it's official and has Chrome
// branding.
base::Optional<version_info::Channel> GetReleaseChannel() {
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return chrome::GetChannel();
#else
  return base::nullopt;
#endif
}

}  // namespace

ThreadProfilerConfiguration::ThreadProfilerConfiguration()
    : current_process_(
          GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess())),
      platform_configuration_(ThreadProfilerPlatformConfiguration::Create(
          IsBrowserTestModeEnabled())),
      configuration_(
          GenerateConfiguration(current_process_, *platform_configuration_)) {}

ThreadProfilerConfiguration::~ThreadProfilerConfiguration() = default;

base::StackSamplingProfiler::SamplingParams
ThreadProfilerConfiguration::GetSamplingParams() const {
  base::StackSamplingProfiler::SamplingParams params;
  params.initial_delay = base::TimeDelta::FromMilliseconds(0);
  // Trim the sampling duration when testing the profiler using browser tests.
  // The standard 30 second duration risks flaky timeouts since it's close to
  // the test timeout of 45 seconds.
  const base::TimeDelta duration =
      base::TimeDelta::FromSeconds(IsBrowserTestModeEnabled() ? 1 : 30);
  params.sampling_interval = base::TimeDelta::FromMilliseconds(100);
  params.samples_per_profile = duration / params.sampling_interval;

  return params;
}

bool ThreadProfilerConfiguration::IsProfilerEnabledForCurrentProcess() const {
  if (current_process_ == metrics::CallStackProfileParams::BROWSER_PROCESS) {
    return configuration_ == PROFILE_ENABLED ||
           configuration_ == PROFILE_CONTROL;
  }

  DCHECK_EQ(PROFILE_FROM_COMMAND_LINE, configuration_);
  // This is a child process. The |kStartStackProfiler| switch passed by the
  // browser process determines whether the profiler is enabled for the process.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kStartStackProfiler);
}

bool ThreadProfilerConfiguration::IsProfilerEnabledForCurrentProcessAndThread(
    metrics::CallStackProfileParams::Thread thread) const {
  return IsProfilerEnabledForCurrentProcess() &&
         platform_configuration_->IsEnabledForThread(current_process_, thread);
}

bool ThreadProfilerConfiguration::GetSyntheticFieldTrial(
    std::string* trial_name,
    std::string* group_name) const {
  DCHECK_EQ(metrics::CallStackProfileParams::BROWSER_PROCESS, current_process_);

  if (!platform_configuration_->IsSupported(GetReleaseChannel())) {
    return false;
  }

  *trial_name = "SyntheticStackProfilingConfiguration";
  *group_name = std::string();
  switch (configuration_) {
    case PROFILE_DISABLED:
      *group_name = "Disabled";
      break;

    case PROFILE_DISABLED_MODULE_NOT_INSTALLED:
      *group_name = "DisabledModuleNotInstalled";
      break;

    case PROFILE_CONTROL:
      *group_name = "Control";
      break;

    case PROFILE_ENABLED:
      *group_name = "Enabled";
      break;

    case PROFILE_FROM_COMMAND_LINE:
      NOTREACHED();
      break;
  }

  return !group_name->empty();
}

void ThreadProfilerConfiguration::AppendCommandLineSwitchForChildProcess(
    base::CommandLine* child_process_command_line) const {
  DCHECK_EQ(metrics::CallStackProfileParams::BROWSER_PROCESS, current_process_);

  if (!(configuration_ == PROFILE_ENABLED || configuration_ == PROFILE_CONTROL))
    return;

  const metrics::CallStackProfileParams::Process child_process =
      GetProfileParamsProcess(*child_process_command_line);
  const double enable_fraction =
      platform_configuration_->GetChildProcessEnableFraction(child_process);
  if (!(base::RandDouble() < enable_fraction))
    return;

  if (IsBrowserTestModeEnabled()) {
    // Propagate the browser test mode switch argument to the child processes.
    child_process_command_line->AppendSwitchASCII(
        switches::kStartStackProfiler,
        switches::kStartStackProfilerBrowserTest);
  } else {
    child_process_command_line->AppendSwitch(switches::kStartStackProfiler);
  }
}

// static
ThreadProfilerConfiguration* ThreadProfilerConfiguration::Get() {
  return g_configuration.Pointer();
}

// static
ThreadProfilerConfiguration::ProfileConfiguration
ThreadProfilerConfiguration::ChooseConfiguration(
    const std::vector<Variation>& variations) {
  int total_weight = 0;
  for (const Variation& variation : variations)
    total_weight += variation.weight;
  DCHECK_EQ(100, total_weight);

  int chosen = base::RandInt(0, total_weight - 1);  // Max is inclusive.
  int cumulative_weight = 0;
  for (const auto& variation : variations) {
    if (chosen >= cumulative_weight &&
        chosen < cumulative_weight + variation.weight) {
      return variation.config;
    }
    cumulative_weight += variation.weight;
  }
  NOTREACHED();
  return PROFILE_DISABLED;
}

// static
ThreadProfilerConfiguration::ProfileConfiguration
ThreadProfilerConfiguration::GenerateConfiguration(
    metrics::CallStackProfileParams::Process process,
    const ThreadProfilerPlatformConfiguration& platform_configuration) {
  if (process != metrics::CallStackProfileParams::BROWSER_PROCESS)
    return PROFILE_FROM_COMMAND_LINE;

  const base::Optional<version_info::Channel> release_channel =
      GetReleaseChannel();
  if (!platform_configuration.IsSupported(release_channel))
    return PROFILE_DISABLED;

  using RuntimeModuleState =
      ThreadProfilerPlatformConfiguration::RuntimeModuleState;
  switch (platform_configuration.GetRuntimeModuleState(release_channel)) {
    case RuntimeModuleState::kModuleAbsentButAvailable:
      platform_configuration.RequestRuntimeModuleInstall();
      FALLTHROUGH;
    case RuntimeModuleState::kModuleNotAvailable:
      return PROFILE_DISABLED_MODULE_NOT_INSTALLED;

    case RuntimeModuleState::kModuleNotRequired:
    case RuntimeModuleState::kModulePresent:
      break;
  }

  ThreadProfilerPlatformConfiguration::RelativePopulations
      relative_populations =
          platform_configuration.GetEnableRates(release_channel);

  CHECK_EQ(0, relative_populations.experiment % 2);
  return ChooseConfiguration({
      {PROFILE_ENABLED, relative_populations.enabled},
      {PROFILE_CONTROL, relative_populations.experiment / 2},
      {PROFILE_DISABLED, relative_populations.experiment / 2},
  });
}
