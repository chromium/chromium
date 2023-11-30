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

// This enables the CPU performance interventions within the side panel.
BASE_DECLARE_FEATURE(kPerformanceCPUIntervention);

// This represents the duration that CPU must be over the threshold before
// an intervention is triggered.
extern const base::FeatureParam<base::TimeDelta> kCPUTimeOverThreshold;

// If Chrome CPU utilization and System CPU utilization are both over the
// specified percent thresholds then we will trigger an intervention.
extern const base::FeatureParam<int> kCPUSystemPercentThreshold;
extern const base::FeatureParam<int> kCPUChromePercentThreshold;

// This enables the Memory performance interventions within the side panel.
BASE_DECLARE_FEATURE(kPerformanceMemoryIntervention);

// This represents the duration that Memory must be over the threshold before
// an intervention is triggered.
extern const base::FeatureParam<base::TimeDelta> kMemoryTimeOverThreshold;

// If available Memory percent and bytes are both under the specified thresholds
// then we will trigger an intervention.
extern const base::FeatureParam<int> kMemoryFreePercentThreshold;
extern const base::FeatureParam<int> kMemoryFreeBytesThreshold;

#endif

BASE_DECLARE_FEATURE(kPMProcessPriorityPolicy);

extern const base::FeatureParam<bool> kDownvoteAdFrames;

// Enables or disables the availability of the probabilistic proactive tab
// discarding evaluator.
BASE_DECLARE_FEATURE(kProbabilisticProactiveDiscarding);

// The target false positive rate, in percent, of the probabilistic proactive
// tab discarder. For example, if this value is 35, the discarder will attempt
// to discard tabs such that *at most* 35% of discarded tabs are revisited
// within 2 days.
extern const base::FeatureParam<int>
    kProactiveDiscardingTargetFalsePositivePercent;

// The time interval at which the Proactive Discarder evaluates background tabs
// for discard eligibility.
extern const base::FeatureParam<base::TimeDelta>
    kProactiveDiscardingSamplingInterval;

// If true, runs the proactive discard policy in simulation mode (makes
// discarding decisions and tracks success metrics but doesn't discard)
extern const base::FeatureParam<bool> kProactiveDiscardingSimulationMode;

// When enabled, Memory Saver supports the different modes defined in the
// `ModalMemorySaverMode` enum.
BASE_DECLARE_FEATURE(kModalMemorySaver);

// When set, makes Memory Saver behave as the specified mode if it's  enabled.
extern const base::FeatureParam<int> kModalMemorySaverMode;

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
