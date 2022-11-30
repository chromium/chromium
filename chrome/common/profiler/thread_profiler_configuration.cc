// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_configuration.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_platform_configuration.h"
#include "chrome/common/profiler/unwind_util.h"
#include "components/version_info/version_info.h"

namespace {

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
absl::optional<version_info::Channel> GetReleaseChannel() {
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return chrome::GetChannel();
#else
  return absl::nullopt;
#endif
}

}  // namespace

// static
ThreadProfilerConfiguration* ThreadProfilerConfiguration::Get() {
  static base::NoDestructor<ThreadProfilerConfiguration>
      thread_profiler_configuration;
  return thread_profiler_configuration.get();
}

base::StackSamplingProfiler::SamplingParams
ThreadProfilerConfiguration::GetSamplingParams() const {
  base::StackSamplingProfiler::SamplingParams params;
  params.initial_delay = base::Milliseconds(0);
  // Trim the sampling duration when testing the profiler using browser tests.
  // The standard 30 second duration risks flaky timeouts since it's close to
  // the test timeout of 45 seconds.
  const base::TimeDelta duration =
      base::Seconds(IsBrowserTestModeEnabled() ? 1 : 30);
  params.sampling_interval = base::Milliseconds(100);
  params.samples_per_profile = duration / params.sampling_interval;

  return params;
}

bool ThreadProfilerConfiguration::IsProfilerEnabledForCurrentProcess() const {
  if (const ChildProcessConfiguration* child_process_configuration =
          absl::get_if<ChildProcessConfiguration>(&configuration_)) {
    return *child_process_configuration == kChildProcessProfileEnabled;
  }

  const absl::optional<VariationGroup>& variation_group =
      absl::get<BrowserProcessConfiguration>(configuration_);

  return EnableForVariationGroup(variation_group);
}

bool ThreadProfilerConfiguration::IsProfilerEnabledForCurrentProcessAndThread(
    metrics::CallStackProfileParams::Thread thread) const {
  return IsProfilerEnabledForCurrentProcess() &&
         platform_configuration_->IsEnabledForThread(
             GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess()),
             thread);
}

bool ThreadProfilerConfiguration::GetSyntheticFieldTrial(
    std::string* trial_name,
    std::string* group_name) const {
  DCHECK(absl::holds_alternative<BrowserProcessConfiguration>(configuration_));
  const absl::optional<VariationGroup>& variation_group =
      absl::get<BrowserProcessConfiguration>(configuration_);

  if (!variation_group.has_value())
    return false;

  *trial_name = "SyntheticStackProfilingConfiguration";
  *group_name = std::string();
  switch (*variation_group) {
    case kProfileDisabled:
      *group_name = "Disabled";
      break;

    case kProfileDisabledModuleNotInstalled:
      *group_name = "DisabledModuleNotInstalled";
      break;

    case kProfileControl:
      *group_name = "Control";
      break;

    case kProfileEnabled:
      *group_name = "Enabled";
      break;
  }

  return true;
}

void ThreadProfilerConfiguration::AppendCommandLineSwitchForChildProcess(
    base::CommandLine* child_process_command_line) const {
  DCHECK(absl::holds_alternative<BrowserProcessConfiguration>(configuration_));
  const absl::optional<VariationGroup>& variation_group =
      absl::get<BrowserProcessConfiguration>(configuration_);

  if (!EnableForVariationGroup(variation_group))
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

ThreadProfilerConfiguration::ThreadProfilerConfiguration()
    : platform_configuration_(ThreadProfilerPlatformConfiguration::Create(
          IsBrowserTestModeEnabled())),
      configuration_(GenerateConfiguration(
          GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess()),
          *platform_configuration_)) {}

// static
bool ThreadProfilerConfiguration::EnableForVariationGroup(
    absl::optional<VariationGroup> variation_group) {
  // Enable if assigned to a variation group, and the group is one of the groups
  // that are to be enabled.
  return variation_group.has_value() && (*variation_group == kProfileEnabled ||
                                         *variation_group == kProfileControl);
}

// static
ThreadProfilerConfiguration::VariationGroup
ThreadProfilerConfiguration::ChooseVariationGroup(
    std::initializer_list<Variation> variations) {
  int total_weight = 0;
  for (const Variation& variation : variations)
    total_weight += variation.weight;
  DCHECK_EQ(100, total_weight);

  int chosen = base::RandInt(0, total_weight - 1);  // Max is inclusive.
  int cumulative_weight = 0;
  for (const auto& variation : variations) {
    if (chosen >= cumulative_weight &&
        chosen < cumulative_weight + variation.weight) {
      return variation.group;
    }
    cumulative_weight += variation.weight;
  }
  NOTREACHED();
  return kProfileDisabled;
}

// static
ThreadProfilerConfiguration::BrowserProcessConfiguration
ThreadProfilerConfiguration::GenerateBrowserProcessConfiguration(
    const ThreadProfilerPlatformConfiguration& platform_configuration) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableStackProfiler))
    return absl::nullopt;

  const absl::optional<version_info::Channel> release_channel =
      GetReleaseChannel();

  if (!platform_configuration.IsSupported(release_channel))
    return absl::nullopt;

  // We pass `version_info::Channel::UNKNOWN` instead of `absl::nullopt` here
  // because `AreUnwindPrerequisitesAvailable` accounts for official build
  // status internally.
  if (!AreUnwindPrerequisitesAvailable(
          release_channel.value_or(version_info::Channel::UNKNOWN))) {
    return kProfileDisabledModuleNotInstalled;
  }

  ThreadProfilerPlatformConfiguration::RelativePopulations
      relative_populations =
          platform_configuration.GetEnableRates(release_channel);

  CHECK_EQ(0, relative_populations.experiment % 2);
  return ChooseVariationGroup({
      {kProfileEnabled, relative_populations.enabled},
      {kProfileControl, relative_populations.experiment / 2},
      {kProfileDisabled, relative_populations.experiment / 2},
  });
}

// static
ThreadProfilerConfiguration::ChildProcessConfiguration
ThreadProfilerConfiguration::GenerateChildProcessConfiguration(
    const base::CommandLine& command_line) {
  // In a child process the |kStartStackProfiler| switch passed by the
  // browser process determines whether the profiler is enabled for the
  // process.
  return command_line.HasSwitch(switches::kStartStackProfiler)
             ? kChildProcessProfileEnabled
             : kChildProcessProfileDisabled;
}

// static
ThreadProfilerConfiguration::Configuration
ThreadProfilerConfiguration::GenerateConfiguration(
    metrics::CallStackProfileParams::Process process,
    const ThreadProfilerPlatformConfiguration& platform_configuration) {
  if (process == metrics::CallStackProfileParams::Process::kBrowser)
    return GenerateBrowserProcessConfiguration(platform_configuration);

  return GenerateChildProcessConfiguration(
      *base::CommandLine::ForCurrentProcess());
}
