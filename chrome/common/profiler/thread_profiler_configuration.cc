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

  const auto& config = absl::get<BrowserProcessConfiguration>(configuration_);
  return EnableForVariationGroup(config.variation_group) &&
         IsProcessGloballyEnabled(
             config,
             GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess()));
}

bool ThreadProfilerConfiguration::IsProfilerEnabledForCurrentProcessAndThread(
    metrics::CallStackProfileParams::Thread thread) const {
  return IsProfilerEnabledForCurrentProcess() &&
         platform_configuration_->IsEnabledForThread(
             GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess()),
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

#if BUILDFLAG(IS_ANDROID)
    case kProfileEnabledWithJavaNameHashing:
      *group_name = "EnabledWithJavaNameHashing";
      break;
#endif  // BUILDFLAG(IS_ANDROID)

    case kProfileEnabled:
      *group_name = "Enabled";
      break;
  }

  return true;
}

bool ThreadProfilerConfiguration::IsProfilerEnabledForChildProcess(
    metrics::CallStackProfileParams::Process child_process) const {
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
          GetProfileParamsProcess(*child_process_command_line))) {
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

#if BUILDFLAG(IS_ANDROID)
bool ThreadProfilerConfiguration::IsJavaNameHashingEnabled() const {
  // For now, this is only enabled in the browser process, to verify that
  // the java name hashing is working correctly.
  //
  // TODO(crbug.com/1475718): enable this in the other processes too.
  if (const auto* config =
          absl::get_if<BrowserProcessConfiguration>(&configuration_)) {
    return config->variation_group == kProfileEnabledWithJavaNameHashing;
  }

  return false;
}
#endif  // BUILDFLAG(IS_ANDROID)

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
  return variation_group.has_value() &&
         (*variation_group == kProfileEnabled ||
#if BUILDFLAG(IS_ANDROID)
          *variation_group == kProfileEnabledWithJavaNameHashing ||
#endif  // BUILDFLAG(IS_ANDROID)
          *variation_group == kProfileControl);
}

// static
bool ThreadProfilerConfiguration::IsProcessGloballyEnabled(
    const ThreadProfilerConfiguration::BrowserProcessConfiguration& config,
    metrics::CallStackProfileParams::Process process) {
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
    return {absl::nullopt, absl::nullopt};

  const absl::optional<version_info::Channel> release_channel =
      GetReleaseChannel();

  if (!platform_configuration.IsSupported(release_channel))
    return {absl::nullopt, absl::nullopt};

  // We pass `version_info::Channel::UNKNOWN` instead of `absl::nullopt` here
  // because `AreUnwindPrerequisitesAvailable` accounts for official build
  // status internally.
  if (!AreUnwindPrerequisitesAvailable(
          release_channel.value_or(version_info::Channel::UNKNOWN))) {
    return {kProfileDisabledModuleNotInstalled, absl::nullopt};
  }

  ThreadProfilerPlatformConfiguration::RelativePopulations
      relative_populations =
          platform_configuration.GetEnableRates(release_channel);

  const absl::optional<metrics::CallStackProfileParams::Process>
      process_type_to_sample = platform_configuration.ChooseEnabledProcess();

#if BUILDFLAG(IS_ANDROID)
  CHECK_EQ(0, relative_populations.experiment % 3);
  return {ChooseVariationGroup({
              {kProfileEnabled, relative_populations.enabled},
              {kProfileControl, relative_populations.experiment / 3},
              {kProfileEnabledWithJavaNameHashing,
               relative_populations.experiment / 3},
              {kProfileDisabled, relative_populations.experiment / 3},
          }),
          process_type_to_sample};
#else
  CHECK_EQ(0, relative_populations.experiment % 2);
  return {ChooseVariationGroup({
              {kProfileEnabled, relative_populations.enabled},
              {kProfileControl, relative_populations.experiment / 2},
              {kProfileDisabled, relative_populations.experiment / 2},
          }),
          process_type_to_sample};
#endif
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
