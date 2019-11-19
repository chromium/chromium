// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/settings.h"

#include "base/allocator/buildflags.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "components/services/heap_profiling/public/cpp/switches.h"

namespace heap_profiling {

const base::Feature kOOPHeapProfilingFeature{"OOPHeapProfiling",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const char kOOPHeapProfilingFeatureMode[] = "mode";

namespace {

const char kOOPHeapProfilingFeatureStackMode[] = "stack-mode";
const char kOOPHeapProfilingFeatureSampling[] = "sampling";
const char kOOPHeapProfilingFeatureSamplingRate[] = "sampling-rate";

const uint32_t kDefaultSamplingRate = 100000;
const bool kDefaultShouldSample = true;

bool RecordAllAllocationsForStartup() {
  return !base::GetFieldTrialParamByFeatureAsBool(
      kOOPHeapProfilingFeature, kOOPHeapProfilingFeatureSampling,
      kDefaultShouldSample);
}

}  // namespace

Mode GetModeForStartup() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  if (cmdline->HasSwitch("enable-heap-profiling")) {
    LOG(ERROR) << "--enable-heap-profiling is no longer supported. Use "
                  "--memlog instead. See documentation at "
                  "docs/memory/debugging_memory_issues.md";
    return Mode::kNone;
  }

  if (cmdline->HasSwitch(kMemlogMode) ||
      base::FeatureList::IsEnabled(kOOPHeapProfilingFeature)) {
    std::string mode;
    // Respect the commandline switch above the field trial.
    if (cmdline->HasSwitch(kMemlogMode)) {
      mode = cmdline->GetSwitchValueASCII(kMemlogMode);
    } else {
      mode = base::GetFieldTrialParamValueByFeature(
          kOOPHeapProfilingFeature, kOOPHeapProfilingFeatureMode);
    }

    return ConvertStringToMode(mode);
  }
  return Mode::kNone;
#else
  LOG_IF(ERROR, cmdline->HasSwitch(kMemlogMode))
      << "--" << kMemlogMode
      << " specified but it will have no effect because the use_allocator_shim "
      << "is not available in this build.";
  return Mode::kNone;
#endif
}

Mode ConvertStringToMode(const std::string& mode) {
  if (mode == kMemlogModeAll)
    return Mode::kAll;
  if (mode == kMemlogModeAllRenderers)
    return Mode::kAllRenderers;
  if (mode == kMemlogModeManual)
    return Mode::kManual;
  if (mode == kMemlogModeMinimal)
    return Mode::kMinimal;
  if (mode == kMemlogModeBrowser)
    return Mode::kBrowser;
  if (mode == kMemlogModeGpu)
    return Mode::kGpu;
  if (mode == kMemlogModeRendererSampling)
    return Mode::kRendererSampling;
  if (mode == kMemlogModeUtilitySampling)
    return Mode::kUtilitySampling;
  if (mode == kMemlogModeUtilityAndBrowser)
    return Mode::kUtilityAndBrowser;
  DLOG(ERROR) << "Unsupported value: \"" << mode << "\" passed to --"
              << kMemlogMode;
  return Mode::kNone;
}

mojom::StackMode GetStackModeForStartup() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  std::string stack_mode;

  // Respect the commandline switch above the field trial.
  if (cmdline->HasSwitch(kMemlogStackMode)) {
    stack_mode = cmdline->GetSwitchValueASCII(kMemlogStackMode);
  } else {
    stack_mode = base::GetFieldTrialParamValueByFeature(
        kOOPHeapProfilingFeature, kOOPHeapProfilingFeatureStackMode);
    if (stack_mode.empty())
      stack_mode = kMemlogStackModeNative;
  }

  return ConvertStringToStackMode(stack_mode);
}

mojom::StackMode ConvertStringToStackMode(const std::string& input) {
  if (input == kMemlogStackModeNative)
    return mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES;
  if (input == kMemlogStackModeNativeWithThreadNames)
    return mojom::StackMode::NATIVE_WITH_THREAD_NAMES;
  if (input == kMemlogStackModePseudo)
    return mojom::StackMode::PSEUDO;
  if (input == kMemlogStackModeMixed)
    return mojom::StackMode::MIXED;
  DLOG(ERROR) << "Unsupported value: \"" << input << "\" passed to --"
              << kMemlogStackMode;
  return mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES;
}

uint32_t GetSamplingRateForStartup() {
  if (RecordAllAllocationsForStartup())
    return 1;

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kMemlogSamplingRate)) {
    std::string rate_as_string =
        cmdline->GetSwitchValueASCII(kMemlogSamplingRate);
    int rate_as_int = 1;
    if (!base::StringToInt(rate_as_string, &rate_as_int)) {
      LOG(ERROR) << "Could not parse sampling rate: " << rate_as_string;
    }
    if (rate_as_int <= 0) {
      LOG(ERROR) << "Invalid sampling rate: " << rate_as_string;
      rate_as_int = 1;
    }
    return rate_as_int;
  }

  return base::GetFieldTrialParamByFeatureAsInt(
      kOOPHeapProfilingFeature, kOOPHeapProfilingFeatureSamplingRate,
      kDefaultSamplingRate);
}

bool IsBackgroundHeapProfilingEnabled() {
  return base::FeatureList::IsEnabled(kOOPHeapProfilingFeature);
}

}  // namespace heap_profiling
