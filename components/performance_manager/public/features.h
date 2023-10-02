// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace performance_manager::features {

// If enabled the PM runs on the main (UI) thread. Incompatible with
// kRunOnDedicatedThreadPoolThread.
BASE_DECLARE_FEATURE(kRunOnMainThread);

// If enabled the PM runs on a single ThreadPool thread that isn't shared with
// any other task runners. It will be named "Performance Manager" in traces.
// This makes it easy to identify tasks running on the PM sequence, but may not
// perform as well as a shared sequence, which is the default. Incompatible with
// kRunOnMainThread.
BASE_DECLARE_FEATURE(kRunOnDedicatedThreadPoolThread);

#if !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX)
#define URGENT_DISCARDING_FROM_PERFORMANCE_MANAGER() false
#else
#define URGENT_DISCARDING_FROM_PERFORMANCE_MANAGER() true
#endif

// Enables urgent discarding of pages directly from PerformanceManager rather
// than via TabManager on Ash Chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_DECLARE_FEATURE(kAshUrgentDiscardingFromPerformanceManager);
#endif

// Enable background tab loading of pages (restored via session restore)
// directly from Performance Manager rather than via TabLoader.
BASE_DECLARE_FEATURE(kBackgroundTabLoadingFromPerformanceManager);

// Make the Battery Saver Modes available to users. If this is enabled, it
// doesn't mean the mode is enabled, just that the user has the option of
// toggling it.
BASE_DECLARE_FEATURE(kBatterySaverModeAvailable);

// Flag to control a baseline HaTS survey for Chrome performance.
BASE_DECLARE_FEATURE(kPerformanceControlsPerformanceSurvey);
BASE_DECLARE_FEATURE(kPerformanceControlsBatteryPerformanceSurvey);
BASE_DECLARE_FEATURE(kPerformanceControlsHighEfficiencyOptOutSurvey);
BASE_DECLARE_FEATURE(kPerformanceControlsBatterySaverOptOutSurvey);

// Defines the time delta to look back when checking if a device has used
// battery.
extern const base::FeatureParam<base::TimeDelta>
    kPerformanceControlsBatterySurveyLookback;

// Controls whether HeuristicMemorySaverPolicy is used as the memory saver
// policy:
//
// * If neither this nor kHighEfficiencyMultistateMode are enabled, the
//   timer-based memory saver policy is used.
// * If this is enabled but kHighEfficiencyMultistateMode is not, the heuristic
//   memory saver is used, with parameters set from the feature params.
// * If this and kHighEfficiencyMultistateMode are both enabled, the policy
//   that's used is controlled by the multistate UI, and the heuristic policy
//   parameters are set from the feature params.
// * If kHighEfficiencyMultistateMode is enabled and this is not, heuristic
//   memory saver is enabled if chosen by the user, but the policy params use
//   default values.
BASE_DECLARE_FEATURE(kHeuristicMemorySaver);

// Controls the interval at which HeuristicMemorySaverPolicy checks whether the
// amount of available memory is smaller than the discarding threshold. The
// "ThresholdReached" version is used when the device is past the threshold
// specified by `kHeuristicMemorySaverAvailableMemoryThresholdPercent` and the
// "ThresholdNotReached" version is used otherwise.
extern const base::FeatureParam<base::TimeDelta>
    kHeuristicMemorySaverThresholdReachedHeartbeatInterval;
extern const base::FeatureParam<base::TimeDelta>
    kHeuristicMemorySaverThresholdNotReachedHeartbeatInterval;

// The amount of available physical memory at which
// HeuristicMemorySaverPolicy will start discarding tabs. The amount of
// available memory must be such that it's both lower than the "Percent" param
// when expressed as a % of total installed physical memory and lower than the
// "Mb" threshold.
//
// For example, if the params are set as:
// - kHeuristicMemorySaverAvailableMemoryThresholdPercent to 20%
// - kHeuristicMemorySaverAvailableMemoryThresholdMb to 2048
//
// A device with 8Gb of installed RAM, 1Gb of which is available is under the
// threshold and will discard tabs (12.5% available and 1Gb < 2048Mb)
//
// A device with 16Gb of installed RAM, 3Gb of which are available is under
// the percentage threshold but will not discard tabs because it's above the
// absolute Mb threshold (18.75% available, but 3Gb > 2048Mb)
extern const base::FeatureParam<int>
    kHeuristicMemorySaverAvailableMemoryThresholdPercent;
extern const base::FeatureParam<int>
    kHeuristicMemorySaverAvailableMemoryThresholdMb;

// The percentage of the page cache that should be considered "available" for
// the purposes of Memory Saver thresholding. For instance, setting this
// parameter to 20 will make it so that 20% of the page cache is added to the
// "free" memory figure on macOS. See the comment in
// `HeuristicMemorySaverPolicy::DefaultGetAmountOfAvailablePhysicalMemory` for
// more information.
extern const base::FeatureParam<int> kHeuristicMemorySaverPageCacheDiscountMac;

// The minimum amount of time a tab has to spend in the background before
// HeuristicMemorySaverPolicy will consider it eligible for discarding.
extern const base::FeatureParam<base::TimeDelta>
    kHeuristicMemorySaverMinimumTimeInBackground;

// If enabled, the kHeuristicMemorySaver feature controls the memory saver
// policy regardless of the state of the UI:
//
// * If kHeuristicMemorySaver is enabled, HeuristicMemorySaverPolicy will be
//   turned on.
// * If kHeuristicMemorySaver is disabled, all memory saver policies will be
//   turned off (for the control group of experiments).
//
// This only affects the original UI. If the experimental multistate UI is
// enabled (through the kHighEfficiencyMultistateMode feature), this is not
// checked.
//
// Note: to get uniform control and experiment groups,
// kForceHeuristicMemorySaver should only be tested for users who are valid for
// randomized studies (eg. not for users with the HighEfficiencyModeState pref
// managed by enterprise policy). It's always safe to read kHeuristicMemorySaver
// without kForceHeuristicMemorySaver to get the policy params.
//
// To run an A/B experiment, the following feature configs should be used:
//
// * Experiment group(s):
//   * kForceHeuristicMemorySaver ON
//   * kHeuristicMemorySaver ON (with parameters to control the policy)
//   * kHighEfficiencyMultistateMode OFF
// * Control group:
//   * kForceHeuristicMemorySaver ON
//   * kHeuristicMemorySaver OFF
//   * kHighEfficiencyMultistateMode OFF
//
// Users who enable kHighEfficiencyMultistateMode through chrome://flags will
// be excluded from the experiment.
BASE_DECLARE_FEATURE(kForceHeuristicMemorySaver);

// Round 2 Performance Controls features

// This enables the UI for the multi-state version of high efficiency mode.
BASE_DECLARE_FEATURE(kHighEfficiencyMultistateMode);
// When true, a recommended badge will be shown next to the heuristic memory
// saver option.
extern const base::FeatureParam<bool> kHighEfficiencyShowRecommendedBadge;

// This shows more information about discarded tabs in the tab strip and
// hovercards.
BASE_DECLARE_FEATURE(kDiscardedTabTreatment);
// This displays active memory usage in hovercards.
BASE_DECLARE_FEATURE(kMemoryUsageInHovercards);
// This enables improved UI for adding site exceptions for tab discarding.
BASE_DECLARE_FEATURE(kDiscardExceptionsImprovements);
// This enables improved UI for highlighting memory savings in the page action
// chip and dialog.
BASE_DECLARE_FEATURE(kMemorySavingsReportingImprovements);

// The minimum time between instances where the chip is shown in expanded mode.
extern const base::FeatureParam<base::TimeDelta>
    kExpandedHighEfficiencyChipFrequency;

// The minimum discard savings that a tab must have for the chip to be expanded.
extern const base::FeatureParam<int> kExpandedHighEfficiencyChipThresholdBytes;

// The minimum time a tab must be discarded before the chip can be shown
// expanded.
extern const base::FeatureParam<base::TimeDelta>
    kExpandedHighEfficiencyChipDiscardedDuration;

// Percentiles of PMF across all tabs on all browsers.
extern const base::FeatureParam<int> kHighEfficiencyChartPmf25PercentileBytes;
extern const base::FeatureParam<int> kHighEfficiencyChartPmf50PercentileBytes;
extern const base::FeatureParam<int> kHighEfficiencyChartPmf75PercentileBytes;
extern const base::FeatureParam<int> kHighEfficiencyChartPmf99PercentileBytes;

// Final opacity of the favicon after the discard animation completes
extern const base::FeatureParam<double> kDiscardedTabTreatmentOpacity;

// The version of the tab discard treatment on the favicon should be shown
extern const base::FeatureParam<int> kDiscardedTabTreatmentOption;

// Threshold for when memory usage is labeled as "high".
extern const base::FeatureParam<int> kMemoryUsageInHovercardsHighThresholdBytes;

// Options for when memory usage metrics are fetched for hovercards.
enum class MemoryUsageInHovercardsUpdateTrigger {
  kBackground,  // Metrics are fetched in the background every 2 minutes
                // (default).
  kNavigation,  // Metrics are also fetched after a navigation becomes idle.
};

// Sets when memory usage metrics will be fetched to display in hovercards.
extern const base::FeatureParam<MemoryUsageInHovercardsUpdateTrigger>
    kMemoryUsageInHovercardsUpdateTrigger;

enum class DiscardTabTreatmentOptions {
  kNone = 0,
  kFadeFullsizedFavicon = 1,
  kFadeSmallFaviconWithRing = 2
};

// This enables the performance controls side panel for learning about and
// configuring performance settings.
BASE_DECLARE_FEATURE(kPerformanceControlsSidePanel);

#endif

// Policy that evicts the BFCache of pages that become non visible or the
// BFCache of all pages when the system is under memory pressure.
BASE_DECLARE_FEATURE(kBFCachePerformanceManagerPolicy);

// Whether tabs are discarded under high memory pressure.
BASE_DECLARE_FEATURE(kUrgentPageDiscarding);

// Enable PageTimelineMonitor timer and by extension, PageTimelineState event
// collection.
BASE_DECLARE_FEATURE(kPageTimelineMonitor);

// Set the interval in seconds between calls of
// PageTimelineMonitor::CollectSlice()
extern const base::FeatureParam<base::TimeDelta> kPageTimelineStateIntervalTime;

// Whether to use the resource_attribution::CPUMeasurementMonitor for logging
// UKM.
extern const base::FeatureParam<bool> kUseResourceAttributionCPUMonitor;

// This enables logging to evaluate the efficacy of potential CPU interventions.
BASE_DECLARE_FEATURE(kCPUInterventionEvaluationLogging);

// This represents the duration that CPU must be over the threshold before
// logging the delayed metrics.
extern const base::FeatureParam<base::TimeDelta> kDelayBeforeLogging;

// If Chrome CPU utilization is over the specified percent then we will log it.
extern const base::FeatureParam<int> kThresholdChromeCPUPercent;

}  // namespace performance_manager::features

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
