// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header contains field trial and variations definitions for policies,
// mechanisms and features in the performance_manager component.

#include "components/performance_manager/public/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace performance_manager::features {

BASE_FEATURE(kRunOnMainThread,
             "RunOnMainThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRunOnDedicatedThreadPoolThread,
             "RunOnDedicatedThreadPoolThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kBackgroundTabLoadingFromPerformanceManager,
             "BackgroundTabLoadingFromPerformanceManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBatterySaverModeAvailable,
             "BatterySaverModeAvailable",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceControlsPerformanceSurvey,
             "PerformanceControlsPerformanceSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceControlsBatteryPerformanceSurvey,
             "PerformanceControlsBatteryPerformanceSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceControlsHighEfficiencyOptOutSurvey,
             "PerformanceControlsHighEfficiencyOptOutSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceControlsBatterySaverOptOutSurvey,
             "PerformanceControlsBatterySaverOptOutSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kPerformanceControlsBatterySurveyLookback{
        &kPerformanceControlsBatteryPerformanceSurvey, "battery_lookback",
        base::Days(8)};

// On ChromeOS, the adjustment generally seems to be around 3%, sometimes 2%. We
// choose 3% because it gets us close enough, or overestimates (which is better
// than underestimating in this instance).
const base::FeatureParam<int>
    kBatterySaverModeThresholdAdjustmentForDisplayLevel {
  &kBatterySaverModeAvailable, "low_battery_threshold_adjustment",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      3,
#else
      0,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

BASE_FEATURE(kHeuristicMemorySaver,
             "HeuristicMemorySaver",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int>
    kHeuristicMemorySaverThresholdReachedHeartbeatSeconds{
        &kHeuristicMemorySaver, "threshold_reached_heartbeat_seconds", 10};
const base::FeatureParam<int>
    kHeuristicMemorySaverThresholdNotReachedHeartbeatSeconds{
        &kHeuristicMemorySaver, "threshold_not_reached_heartbeat_seconds", 60};

const base::FeatureParam<int>
    kHeuristicMemorySaverAvailableMemoryThresholdPercent{
        &kHeuristicMemorySaver, "threshold_percent", 5};

const base::FeatureParam<int> kHeuristicMemorySaverAvailableMemoryThresholdMb{
    &kHeuristicMemorySaver, "threshold_mb", 4096};

const base::FeatureParam<int> kHeuristicMemorySaverPageCacheDiscountMac{
    &kHeuristicMemorySaver, "mac_page_cache_available_percent", 50};

const base::FeatureParam<int> kHeuristicMemorySaverMinimumMinutesInBackground{
    &kHeuristicMemorySaver, "minimum_minutes_in_background", 120};

BASE_FEATURE(kHighEfficiencyMultistateMode,
             "HighEfficiencyMultistateMode",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDiscardedTabTreatment,
             "DiscardedTabTreatment",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMemoryUsageInHovercards,
             "MemoryUsageInHovercards",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDiscardExceptionsImprovements,
             "DiscardExceptionsImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMemorySavingsReportingImprovements,
             "MemorySavingsReportingImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kExpandedHighEfficiencyChipFrequency{
    &kMemorySavingsReportingImprovements,
    "expanded_high_efficiency_chip_frequency", base::Days(1)};

const base::FeatureParam<int> kExpandedHighEfficiencyChipThresholdBytes{
    &kMemorySavingsReportingImprovements,
    "expanded_high_efficiency_chip_threshold_bytes", 200 * 1024 * 1024};

const base::FeatureParam<base::TimeDelta>
    kExpandedHighEfficiencyChipDiscardedDuration{
        &kMemorySavingsReportingImprovements,
        "expanded_high_efficiency_chip_discarded_duration", base::Hours(6)};

#endif

BASE_FEATURE(kBFCachePerformanceManagerPolicy,
             "BFCachePerformanceManagerPolicy",
#if !BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kUrgentPageDiscarding,
             "UrgentPageDiscarding",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageTimelineMonitor,
             "PageTimelineMonitor",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kPageTimelineStateIntervalTime{
    &kPageTimelineMonitor, "time_between_collect_slice", base::Minutes(5)};

}  // namespace performance_manager::features
