// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_configuration.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/thread_profiler_platform_configuration.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "sandbox/policy/sandbox.h"

#if defined(OS_WIN)
#include "base/win/static_constants.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
#endif

namespace {

base::LazyInstance<ThreadProfilerConfiguration>::Leaky g_configuration =
    LAZY_INSTANCE_INITIALIZER;

// Returns true if the current execution is taking place in the browser process.
bool IsBrowserProcess() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  return process_type.empty();
}

// True if the command line corresponds to an extension renderer process.
bool IsExtensionRenderer(const base::CommandLine& command_line) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return command_line.HasSwitch(extensions::switches::kExtensionProcess);
#else
  return false;
#endif
}

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

bool ShouldEnableProfilerForNextRendererProcess() {
  // Ensure deterministic behavior for testing the profiler itself.
  if (IsBrowserTestModeEnabled())
    return true;

  // Enable for every N-th renderer process, where N = 5.
  return base::RandInt(0, 4) == 0;
}

}  // namespace

ThreadProfilerConfiguration::ThreadProfilerConfiguration()
    : platform_configuration_(ThreadProfilerPlatformConfiguration::Create(
          IsBrowserTestModeEnabled())),
      configuration_(GenerateConfiguration(*platform_configuration_)) {}

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
  if (IsBrowserProcess()) {
    return configuration_ == PROFILE_ENABLED ||
           configuration_ == PROFILE_CONTROL;
  }

  DCHECK_EQ(PROFILE_FROM_COMMAND_LINE, configuration_);
  // This is a child process. The |kStartStackProfiler| switch passed by the
  // browser process determines whether the profiler is enabled for the process.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kStartStackProfiler);
}

bool ThreadProfilerConfiguration::GetSyntheticFieldTrial(
    std::string* trial_name,
    std::string* group_name) const {
  DCHECK(IsBrowserProcess());

  if (!platform_configuration_->IsSupported(BUILDFLAG(GOOGLE_CHROME_BRANDING),
                                            chrome::GetChannel())) {
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
    const std::string& process_type,
    base::CommandLine* command_line) const {
  DCHECK(IsBrowserProcess());

  bool enable =
      configuration_ == PROFILE_ENABLED || configuration_ == PROFILE_CONTROL;
  if (!enable)
    return;
  if (process_type == switches::kGpuProcess ||
      (process_type == switches::kUtilityProcess &&
       // The network service is the only utility process that is profiled for
       // now.
       sandbox::policy::SandboxTypeFromCommandLine(*command_line) ==
           sandbox::policy::SandboxType::kNetwork) ||
      (process_type == switches::kRendererProcess &&
       // Do not start the profiler for extension processes since profiling the
       // compositor thread in them is not useful.
       !IsExtensionRenderer(*command_line) &&
       ShouldEnableProfilerForNextRendererProcess())) {
    if (IsBrowserTestModeEnabled()) {
      // Propagate the browser test mode switch argument to the child processes.
      command_line->AppendSwitchASCII(switches::kStartStackProfiler,
                                      switches::kStartStackProfilerBrowserTest);
    } else {
      command_line->AppendSwitch(switches::kStartStackProfiler);
    }
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
    const ThreadProfilerPlatformConfiguration& platform_configuration) {
  if (!IsBrowserProcess())
    return PROFILE_FROM_COMMAND_LINE;

  const version_info::Channel channel = chrome::GetChannel();
  if (!platform_configuration.IsSupported(BUILDFLAG(GOOGLE_CHROME_BRANDING),
                                          channel)) {
    return PROFILE_DISABLED;
  }

  using RuntimeModuleState =
      ThreadProfilerPlatformConfiguration::RuntimeModuleState;
  switch (platform_configuration.GetRuntimeModuleState(
      BUILDFLAG(GOOGLE_CHROME_BRANDING), channel)) {
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
      relative_populations = platform_configuration.GetEnableRates(
          BUILDFLAG(GOOGLE_CHROME_BRANDING), channel);
  if (relative_populations.enabled == 0 &&
      relative_populations.experiment == 0) {
    return PROFILE_DISABLED;
  }

  CHECK_EQ(0, relative_populations.experiment % 2);
  return ChooseConfiguration({
      {PROFILE_ENABLED, relative_populations.enabled},
      {PROFILE_CONTROL, relative_populations.experiment / 2},
      {PROFILE_DISABLED, relative_populations.experiment / 2},
  });
}
