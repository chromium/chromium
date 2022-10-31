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

// The feature that gates whether or not the PM runs on the main (UI) thread.
BASE_DECLARE_FEATURE(kRunOnMainThread);

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
