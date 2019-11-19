// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/stack_sampling_configuration.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "base/win/static_constants.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
#endif

namespace {

base::LazyInstance<StackSamplingConfiguration>::Leaky g_configuration =
    LAZY_INSTANCE_INITIALIZER;

// The profiler is currently only implemented for Windows x64 and Mac x64.
bool IsProfilerSupported() {
#if (defined(OS_WIN) && defined(ARCH_CPU_X86_64)) || defined(OS_MACOSX)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Only run on canary and dev.
  const version_info::Channel channel = chrome::GetChannel();
  return channel == version_info::Channel::CANARY ||
         channel == version_info::Channel::DEV;
#else
  return true;
#endif
#else
  return false;
#endif
}

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

#if defined(OS_WIN)
// Checks if Trend Micro DLLs are loaded in process, so we can disable the
// profiler to avoid hitting their performance bug. See
// https://crbug.com/1018291.
bool IsTrendMicroInProcess() {
#if defined(ARCH_CPU_X86_64)
  return (::GetModuleHandle(L"tmmon64.dll") ||
          ::GetModuleHandle(L"tmmonmgr64.dll"));
#else   // defined(ARCH_CPU_X86_64)
  return (::GetModuleHandle(L"tmmon.dll") ||
          ::GetModuleHandle(L"tmmonmgr.dll"));
#endif  // defined(ARCH_CPU_X86_64)
}
#endif  // defined(OS_WIN)

}  // namespace

StackSamplingConfiguration::StackSamplingConfiguration()
    : configuration_(GenerateConfiguration()) {
}

base::StackSamplingProfiler::SamplingParams
StackSamplingConfiguration::GetSamplingParams() const {
  base::StackSamplingProfiler::SamplingParams params;
  params.initial_delay = base::TimeDelta::FromMilliseconds(0);
  // Trim the sampling duration when testing the profiler using browser tests.
  // The standard 30 second duration risks flaky timeouts since it's close to
  // the test timeout of 45 seconds.
  const base::TimeDelta duration =
      base::TimeDelta::FromSeconds(IsBrowserTestModeEnabled() ? 1 : 30);
  params.sampling_interval = base::TimeDelta::FromMilliseconds(100);
  params.samples_per_profile = duration / params.sampling_interval;
  params.keep_consistent_sampling_interval = true;

  return params;
}

bool StackSamplingConfiguration::IsProfilerEnabledForCurrentProcess() const {
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

bool StackSamplingConfiguration::GetSyntheticFieldTrial(
    std::string* trial_name,
    std::string* group_name) const {
  DCHECK(IsBrowserProcess());

  if (!IsProfilerSupported())
    return false;

  *trial_name = "SyntheticStackProfilingConfiguration";
  *group_name = std::string();
  switch (configuration_) {
    case PROFILE_DISABLED:
      *group_name = "Disabled";
      break;

    case PROFILE_DISABLED_TREND_MICRO:
      *group_name = "DisabledTrendMicro";
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

void StackSamplingConfiguration::AppendCommandLineSwitchForChildProcess(
    const std::string& process_type,
    base::CommandLine* command_line) const {
  DCHECK(IsBrowserProcess());

  bool enable =
      configuration_ == PROFILE_ENABLED || configuration_ == PROFILE_CONTROL;
  if (!enable)
    return;
  if (process_type == switches::kGpuProcess ||
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
StackSamplingConfiguration* StackSamplingConfiguration::Get() {
  return g_configuration.Pointer();
}

// static
StackSamplingConfiguration::ProfileConfiguration
StackSamplingConfiguration::ChooseConfiguration(
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
StackSamplingConfiguration::ProfileConfiguration
StackSamplingConfiguration::GenerateConfiguration() {
  if (!IsBrowserProcess())
    return PROFILE_FROM_COMMAND_LINE;

  if (!IsProfilerSupported())
    return PROFILE_DISABLED;

#if defined(OS_WIN)
  // Do not start the profiler when Application Verifier is in use; running them
  // simultaneously can cause crashes and has no known use case.
  if (GetModuleHandleA(base::win::kApplicationVerifierDllName))
    return PROFILE_DISABLED;

  // Do not start the profiler if Trend Micro DLLs are loaded in process to
  // avoid hitting their performance bug.
  // TODO(https://crbug.com/1018291): Remove once Trend Micro's fixes have
  // propagated to customers.
  if (IsTrendMicroInProcess())
    return PROFILE_DISABLED_TREND_MICRO;
#endif

  switch (chrome::GetChannel()) {
    // Enable the profiler unconditionally for development/waterfall builds.
    case version_info::Channel::UNKNOWN:
      return PROFILE_ENABLED;

#if (defined(OS_WIN) && defined(ARCH_CPU_X86_64)) || defined(OS_MACOSX)
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      return ChooseConfiguration({{PROFILE_ENABLED, 80},
                                  {PROFILE_CONTROL, 10},
                                  {PROFILE_DISABLED, 10}});
#endif

    default:
      return PROFILE_DISABLED;
  }
}
