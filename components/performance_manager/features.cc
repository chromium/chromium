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

BASE_FEATURE(kHighEfficiencyMultistateMode,
             "HighEfficiencyMultistateMode",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kHighEfficiencyShowRecommendedBadge{
    &kHighEfficiencyMultistateMode, "show_recommended_badge", false};

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

const base::FeatureParam<int> kHighEfficiencyChartPmf25PercentileBytes{
    &kMemorySavingsReportingImprovements,
    "high_efficiency_chart_pmf_25_percentile_bytes", 62 * 1024 * 1024};
const base::FeatureParam<int> kHighEfficiencyChartPmf50PercentileBytes{
    &kMemorySavingsReportingImprovements,
    "high_efficiency_chart_pmf_50_percentile_bytes", 112 * 1024 * 1024};
const base::FeatureParam<int> kHighEfficiencyChartPmf75PercentileBytes{
    &kMemorySavingsReportingImprovements,
    "high_efficiency_chart_pmf_75_percentile_bytes", 197 * 1024 * 1024};
const base::FeatureParam<int> kHighEfficiencyChartPmf99PercentileBytes{
    &kMemorySavingsReportingImprovements,
    "high_efficiency_chart_pmf_99_percentile_bytes", 800 * 1024 * 1024};

const base::FeatureParam<double> kDiscardedTabTreatmentOpacity{
    &kDiscardedTabTreatment, "discard_tab_treatment_opacity", 0.5};

const base::FeatureParam<int> kDiscardedTabTreatmentOption{
    &kDiscardedTabTreatment, "discard_tab_treatment_option",
    static_cast<int>(DiscardTabTreatmentOptions::kFadeSmallFaviconWithRing)};

const base::FeatureParam<int> kMemoryUsageInHovercardsHighThresholdBytes{
    &kMemoryUsageInHovercards,
    "memory_usage_in_hovercards_high_threshold_bytes", 800 * 1024 * 1024};

// Mapping of enum value to parameter string for "memory_update_trigger" param.
constexpr base::FeatureParam<MemoryUsageInHovercardsUpdateTrigger>::Option
    kMemoryUsageInHovercardsUpdateTriggerOptions[] = {
        {MemoryUsageInHovercardsUpdateTrigger::kBackground, "background"},
        {MemoryUsageInHovercardsUpdateTrigger::kNavigation, "navigation"},
};

const base::FeatureParam<MemoryUsageInHovercardsUpdateTrigger>
    kMemoryUsageInHovercardsUpdateTrigger{
        &kMemoryUsageInHovercards, "memory_update_trigger",
        MemoryUsageInHovercardsUpdateTrigger::kBackground,
        &kMemoryUsageInHovercardsUpdateTriggerOptions};

BASE_FEATURE(kPerformanceControlsSidePanel,
             "PerformanceControlsSidePanel",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kAshUrgentDiscardingFromPerformanceManager,
             "AshUrgentDiscardingFromPerformanceManager",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif

BASE_FEATURE(kBFCachePerformanceManagerPolicy,
             "BFCachePerformanceManagerPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUrgentPageDiscarding,
             "UrgentPageDiscarding",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageTimelineMonitor,
             "PageTimelineMonitor",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kPageTimelineStateIntervalTime{
    &kPageTimelineMonitor, "time_between_collect_slice", base::Minutes(5)};

const base::FeatureParam<bool> kUseResourceAttributionCPUMonitor{
    &kPageTimelineMonitor, "use_resource_attribution_cpu_monitor", false};

BASE_FEATURE(kCPUInterventionEvaluationLogging,
             "CPUInterventionEvaluationLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kDelayBeforeLogging{
    &kCPUInterventionEvaluationLogging, "delay_before_logging",
    base::Seconds(60)};

const base::FeatureParam<int> kThresholdChromeCPUPercent{
    &kCPUInterventionEvaluationLogging, "threshold_chrome_cpu_percent", 25};

}  // namespace performance_manager::features
