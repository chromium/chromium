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

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_LINUX)
#define URGENT_DISCARDING_FROM_PERFORMANCE_MANAGER() false
#else
#define URGENT_DISCARDING_FROM_PERFORMANCE_MANAGER() true
#endif

// Enable background tab loading of pages (restored via session restore)
// directly from Performance Manager rather than via TabLoader.
BASE_DECLARE_FEATURE(kBackgroundTabLoadingFromPerformanceManager);

// Make the High-Efficiency or Battery Saver Modes available to users. If this
// is enabled, it doesn't mean the specific Mode is enabled, just that the user
// has the option of toggling it.
BASE_DECLARE_FEATURE(kHighEfficiencyModeAvailable);
BASE_DECLARE_FEATURE(kBatterySaverModeAvailable);

// Defines the time in seconds before a background tab is discarded for
// High-Efficiency Mode.
extern const base::FeatureParam<base::TimeDelta>
    kHighEfficiencyModeTimeBeforeDiscard;

// The default state of the high-efficiency mode pref
extern const base::FeatureParam<bool> kHighEfficiencyModeDefaultState;

// The number of tabs at which the user may be prompted to enable high
// efficiency mode.
extern const base::FeatureParam<int> kHighEfficiencyModePromoTabCountThreshold;

// The percentage of used memory at which the user may be prompted to enable
// high efficiency mode. For instance, if this parameter is set to 70, the promo
// would be triggered when memory use exceeds 70% of available memory.
extern const base::FeatureParam<int>
    kHighEfficiencyModePromoMemoryPercentThreshold;

// Flag to control a baseline HaTS survey for Chrome performance.
BASE_DECLARE_FEATURE(kPerformanceControlsPerformanceSurvey);
BASE_DECLARE_FEATURE(kPerformanceControlsBatteryPerformanceSurvey);
BASE_DECLARE_FEATURE(kPerformanceControlsHighEfficiencyOptOutSurvey);
BASE_DECLARE_FEATURE(kPerformanceControlsBatterySaverOptOutSurvey);

// Defines the time delta to look back when checking if a device has used
// battery.
extern const base::FeatureParam<base::TimeDelta>
    kPerformanceControlsBatterySurveyLookback;

// On certain platforms (ChromeOS), the battery level displayed to the user is
// artificially lower than the actual battery level. Unfortunately, the battery
// level that Battery Saver Mode looks at is the "actual" level, so users on
// that platform may see Battery Saver Mode trigger at say 17% rather than the
// "advertised" 20%. This parameter allows us to heuristically tweak the
// threshold on those platforms, by being added to the 20% threshold value (so
// setting this parameter to 3 would result in battery saver being activated at
// 23% actual battery level).
extern const base::FeatureParam<int>
    kBatterySaverModeThresholdAdjustmentForDisplayLevel;

// When enabled, the memory saver policy used is HeuristicMemorySaverPolicy.
BASE_DECLARE_FEATURE(kHeuristicMemorySaver);

// Controls the interval at which HeuristicMemorySaverPolicy checks whether the
// amount of available memory is smaller than the discarding threshold. The
// "ThresholdReached" version is used when the device is past the threshold
// specified by `kHeuristicMemorySaverAvailableMemoryThresholdPercent` and the
// "ThresholdNotReached" version is used otherwise.
extern const base::FeatureParam<int>
    kHeuristicMemorySaverThresholdReachedHeartbeatSeconds;
extern const base::FeatureParam<int>
    kHeuristicMemorySaverThresholdNotReachedHeartbeatSeconds;

// The percentage of available physical memory at which
// HeuristicMemorySaverPolicy will start discarding tabs. For example, setting
// this param to 10 will cause HeuristicMemorySaverPolicy to discard tabs
// periodically as long as the available system memory is under 10%.
extern const base::FeatureParam<int>
    kHeuristicMemorySaverAvailableMemoryThresholdPercent;

// The minimum amount of minutes a tab has to spend in the background before
// HeuristicMemorySaverPolicy will consider it eligible for discarding.
extern const base::FeatureParam<int>
    kHeuristicMemorySaverMinimumMinutesInBackground;

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

}  // namespace performance_manager::features

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_FEATURES_H_
