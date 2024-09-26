// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_configuration.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/profiler/stack_sampler.h"
#include "base/rand_util.h"
#include "build/branding_buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/process_type.h"
#include "chrome/common/profiler/thread_profiler_platform_configuration.h"
#include "chrome/common/profiler/unwind_util.h"
#include "components/sampling_profiler/process_type.h"
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
std::optional<version_info::Channel> GetReleaseChannel() {
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return chrome::GetChannel();
#else
  return std::nullopt;
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

  const auto& config = absl::get<BrowserProcessConfiguration>(configuration_);
  return EnableForVariationGroup(config.variation_group) &&
         IsProcessGloballyEnabled(
             config,
             GetProfilerProcessType(*base::CommandLine::ForCurrentProcess()));
}

bool ThreadProfilerConfiguration::IsProfilerEnabledForCurrentProcessAndThread(
    sampling_profiler::ProfilerThreadType thread) const {
  return IsProfilerEnabledForCurrentProcess() &&
         platform_configuration_->IsEnabledForThread(
             GetProfilerProcessType(*base::CommandLine::ForCurrentProcess()),
             thread, GetReleaseChannel());
}

bool ThreadProfilerConfiguration::GetSyntheticFieldTrial(
    std::string* trial_name,
    std::string* group_name) const {
  DCHECK(absl::holds_alternative<BrowserProcessConfiguration>(configuration_));
  const auto& config = absl::get<BrowserProcessConfiguration>(configuration_);

  if (!config.variation_group.has_value()) {
    return false;
  }

  *trial_name = "SyntheticStackProfilingConfiguration";
  *group_name = std::string();
  switch (*config.variation_group) {
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
    case kProfileDisabledOutsideOfExperiment:
      *group_name = "DisabledOutsideOfExperiment";
      break;
  }

  return true;
}

bool ThreadProfilerConfiguration::IsProfilerEnabledForChildProcess(
    sampling_profiler::ProfilerProcessType child_process) const {
  const auto& config = absl::get<BrowserProcessConfiguration>(configuration_);

  const double enable_fraction =
      platform_configuration_->GetChildProcessPerExecutionEnableFraction(
          child_process);
  const bool in_enabled_fraction = base::RandDouble() < enable_fraction;

  return EnableForVariationGroup(config.variation_group) &&
         IsProcessGloballyEnabled(config, child_process) && in_enabled_fraction;
}

void ThreadProfilerConfiguration::AppendCommandLineSwitchForChildProcess(
    base::CommandLine* child_process_command_line) const {
  DCHECK(absl::holds_alternative<BrowserProcessConfiguration>(configuration_));
  if (!IsProfilerEnabledForChildProcess(
          GetProfilerProcessType(*child_process_command_line))) {
    return;
  }

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
          GetProfilerProcessType(*base::CommandLine::ForCurrentProcess()),
          *platform_configuration_)) {
}

// static
bool ThreadProfilerConfiguration::EnableForVariationGroup(
    std::optional<VariationGroup> variation_group) {
  // Enable if assigned to a variation group, and the group is one of the groups
  // that are to be enabled.
  return variation_group.has_value() &&
         (*variation_group == kProfileEnabled ||
          *variation_group == kProfileControl);
}

// static
bool ThreadProfilerConfiguration::IsProcessGloballyEnabled(
    const ThreadProfilerConfiguration::BrowserProcessConfiguration& config,
    sampling_profiler::ProfilerProcessType process) {
  return !config.process_type_to_sample.has_value() ||
         process == *config.process_type_to_sample;
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
  NOTREACHED_IN_MIGRATION();
  return kProfileDisabled;
}

// static
ThreadProfilerConfiguration::BrowserProcessConfiguration
ThreadProfilerConfiguration::GenerateBrowserProcessConfiguration(
    const ThreadProfilerPlatformConfiguration& platform_configuration) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableStackProfiler))
    return {std::nullopt, std::nullopt};

  const std::optional<version_info::Channel> release_channel =
      GetReleaseChannel();

  if (!platform_configuration.IsSupported(release_channel))
    return {std::nullopt, std::nullopt};

  // We pass `version_info::Channel::UNKNOWN` instead of `std::nullopt` here
  // because `AreUnwindPrerequisitesAvailable` accounts for official build
  // status internally.
  if (!AreUnwindPrerequisitesAvailable(
          release_channel.value_or(version_info::Channel::UNKNOWN))) {
    return {kProfileDisabledModuleNotInstalled, std::nullopt};
  }

  ThreadProfilerPlatformConfiguration::RelativePopulations
      relative_populations =
          platform_configuration.GetEnableRates(release_channel);

  const std::optional<sampling_profiler::ProfilerProcessType>
      process_type_to_sample = platform_configuration.ChooseEnabledProcess();

  CHECK_EQ(0, relative_populations.experiment % 2);
  return {
      ChooseVariationGroup({
          {kProfileDisabledOutsideOfExperiment, relative_populations.disabled},
          {kProfileEnabled, relative_populations.enabled},
          {kProfileControl, relative_populations.experiment / 2},
          {kProfileDisabled, relative_populations.experiment / 2},
      }),
      process_type_to_sample};
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
    sampling_profiler::ProfilerProcessType process,
    const ThreadProfilerPlatformConfiguration& platform_configuration) {
  if (process == sampling_profiler::ProfilerProcessType::kBrowser) {
    return GenerateBrowserProcessConfiguration(platform_configuration);
  }

  return GenerateChildProcessConfiguration(
      *base::CommandLine::ForCurrentProcess());
}
