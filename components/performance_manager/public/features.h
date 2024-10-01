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

// If enabled, the PM runs on the main (UI) thread *and* tasks posted to the PM
// TaskRunner from the main (UI) thread run synchronously.
BASE_DECLARE_FEATURE(kRunOnMainThreadSync);

#if !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX)
#define URGENT_DISCARDING_FROM_PERFORMANCE_MANAGER() false
#else
#define URGENT_DISCARDING_FROM_PERFORMANCE_MANAGER() true
#endif

// When enabled removes the rate limit on reporting tab processes to resourced.
#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kUnthrottledTabProcessReporting);
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
BASE_DECLARE_FEATURE(kPerformanceControlsMemorySaverOptOutSurvey);
BASE_DECLARE_FEATURE(kPerformanceControlsBatterySaverOptOutSurvey);

// Defines the time delta to look back when checking if a device has used
// battery.
extern const base::FeatureParam<base::TimeDelta>
    kPerformanceControlsBatterySurveyLookback;

// Round 3 Performance Controls features

// This enables the performance detection backend.
BASE_DECLARE_FEATURE(kPerformanceIntervention);

// This enables the performance intervention UI
BASE_DECLARE_FEATURE(kPerformanceInterventionUI);

// This enables performance intervention to run in demo mode. While in demo
// mode, performance intervention will ignore rate throttling and CPU thresholds
// to make it easier to trigger performance intervention for testing purposes.
BASE_DECLARE_FEATURE(kPerformanceInterventionDemoMode);

bool ShouldUsePerformanceInterventionBackend();

// This represents the version number for the string displayed on the
// Performance Intervention Dialog.
extern const base::FeatureParam<int> kInterventionDialogStringVersion;

// This represents whether we should show the performance intervention
// UI when the suggested tabs to take action on include tabs from a
// profile that is different from the last active browser.
extern const base::FeatureParam<bool> kInterventionShowMixedProfileSuggestions;

#if BUILDFLAG(IS_WIN)
// Prefetch the main browser DLL when a new node is added to the PM graph
// and no prefetch has been done within a reasonable timeframe.
BASE_DECLARE_FEATURE(kPrefetchVirtualMemoryPolicy);
#endif

// This represents the duration that the performance intervention button
// should remain in the toolbar after the user dismisses the intervention
// dialog without taking the suggested action.
extern const base::FeatureParam<base::TimeDelta> kInterventionButtonTimeout;

// This represents the duration that CPU must be over the threshold before
// a notification is triggered.
extern const base::FeatureParam<base::TimeDelta> kCPUTimeOverThreshold;

// Frequency to sample for cpu usage to ensure that the user is experiencing
// consistent cpu issues before surfacing a notification
extern const base::FeatureParam<base::TimeDelta> kCPUSampleFrequency;

// If the system CPU consistently exceeds these percent thresholds, then
// the CPU health will be classified as the threshold it is exceeding
extern const base::FeatureParam<int> kCPUDegradedHealthPercentageThreshold;
extern const base::FeatureParam<int> kCPUUnhealthyPercentageThreshold;

// Maximum number of tabs to be actionable
extern const base::FeatureParam<int> kCPUMaxActionableTabs;

// Minimum percentage to improve CPU health for a tab to be actionable
extern const base::FeatureParam<int> kMinimumActionableTabCPUPercentage;

// This represents the duration that Memory must be over the threshold before
// a notification is triggered.
extern const base::FeatureParam<base::TimeDelta> kMemoryTimeOverThreshold;

// If available Memory percent and bytes are both under the specified thresholds
// then we will trigger a notification.
extern const base::FeatureParam<int> kMemoryFreePercentThreshold;
extern const base::FeatureParam<int> kMemoryFreeBytesThreshold;

#endif

BASE_DECLARE_FEATURE(kPMProcessPriorityPolicy);

extern const base::FeatureParam<bool> kInheritParentPriority;

extern const base::FeatureParam<bool> kDownvoteAdFrames;

BASE_DECLARE_FEATURE(kPMLoadingPageVoter);

// Policy that evicts the BFCache of pages that become non visible or the
// BFCache of all pages when the system is under memory pressure.
BASE_DECLARE_FEATURE(kBFCachePerformanceManagerPolicy);

// Whether tabs are discarded under high memory pressure.
BASE_DECLARE_FEATURE(kUrgentPageDiscarding);

// This represents the duration that CPU must be over the threshold before
// logging the delayed metrics.
extern const base::FeatureParam<base::TimeDelta> kDelayBeforeLogging;

// If Chrome CPU utilization is over the specified percent then we will log it.
extern const base::FeatureParam<int> kThresholdChromeCPUPercent;

// When enabled, the freezing policy measures background CPU usage.
BASE_DECLARE_FEATURE(kCPUMeasurementInFreezingPolicy);

// Proportion of background CPU usage for a group of frames/workers that belong
// to the same [browsing instance, origin] that is considered "high".
BASE_DECLARE_FEATURE_PARAM(double, kFreezingHighCPUProportion);

// Time for which a page cannot be frozen after being visible.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kFreezingVisibleProtectionTime);

// Time for which a page cannot be frozen after being audible.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kFreezingAudioProtectionTime);

// When enabled, browsing instances with high CPU usage in background are frozen
// when Battery Saver is active. Depends on kCPUMeasurementInFreezingPolicy.
BASE_DECLARE_FEATURE(kFreezingOnBatterySaver);

// This is the similar to `kFreezingOnBatterySaver`, with some changes to
// facilitate testing:
// - Pretend that Battery Saver is active even if it's not.
// - Pretend that all tabs have high CPU usage in background.
BASE_DECLARE_FEATURE(kFreezingOnBatterySaverForTesting);

// When enabled, Resource Attribution measurements will include contexts for
// individual origins.
BASE_DECLARE_FEATURE(kResourceAttributionIncludeOrigins);

// When enabled, change the ordering of frame swap in render (crbug/357649043).
BASE_DECLARE_FEATURE(kSeamlessRenderFrameSwap);

// When enabled, visible unimportant frames receives a lesser priority than
// non unimportant frames.
BASE_DECLARE_FEATURE(kUnimportantFramesPriority);

// When enabled, PerformanceManager will update
// blink::performance_scenarios::LoadingScenario.
BASE_DECLARE_FEATURE(kLoadingPerformanceScenario);

}  // namespace performance_manager::features

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
