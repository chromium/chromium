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

namespace performance_manager::features {

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kBackgroundTabLoadingFromPerformanceManager,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceControlsPPMSurvey, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kPerformanceControlsPPMSurveyMinDelay,
                   &kPerformanceControlsPPMSurvey,
                   "ppm_survey_min_delay",
                   base::Minutes(2));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kPerformanceControlsPPMSurveyMaxDelay,
                   &kPerformanceControlsPPMSurvey,
                   "ppm_survey_max_delay",
                   base::Minutes(60));

BASE_FEATURE_PARAM(bool,
                   kPerformanceControlsPPMSurveyUniformSampleValue,
                   &kPerformanceControlsPPMSurvey,
                   "ppm_survey_uniform_sample",
                   true);

// Depending on platform, clients will be split into 1-3 segments based on the
// amount of physical RAM they have. "ppm_survey_segment_name1" through
// "ppm_survey_segment_name3" give the names of the segments, which will be
// included in the PPM survey string data.
//
// "ppm_survey_segment_max_memory_gb1" and "ppm_survey_segment_max_memory_gb2"
// define the upper bounds of segments 1 and 2. The lower bound of segment 1 is
// always 0 GB; if "ppm_survey_segment_max_memory_gb1" is 0, it has no upper
// bound so it's the only defined segment ("ppm_survey_segment_name2", etc, are
// ignored). Otherwise "ppm_survey_segment_max_memory_gb1" is the upper bound
// (inclusive) of segment 1 and the lower bound (exclusive) of segment 2.
//
// Likewise, if "ppm_survey_segment_max_memory_gb2" is 0, segment 2 has no upper
// bound so this platform has only 2 defined segments. Otherwise
// "ppm_survey_segment_max_memory_gb2" is the upper bound (inclusive) of segment
// 2 and the lower bound (exclusive) of segment 3. Segment 3 is the last segment
// that can be defined so it never has an upper bound.
//
// Comparing the client's physical RAM to the boundaries of each defined segment
// determines which one the client falls into. The default parameters give the
// trivial case with only 1 segment containing all users.
//
// If the name parameter of the client's segment is an empty string, that
// segment has already received enough survey responses so clients in that
// segment should not see the survey.
BASE_FEATURE_PARAM(std::string,
                   kPerformanceControlsPPMSurveySegmentName1,
                   &kPerformanceControlsPPMSurvey,
                   "ppm_survey_segment_name1",
                   // All clients fall into this segment when
                   // "ppm_survey_segment_max_memory_gb1" is 0.
                   "Default");
BASE_FEATURE_PARAM(std::string,
                   kPerformanceControlsPPMSurveySegmentName2,
                   &kPerformanceControlsPPMSurvey,
                   "ppm_survey_segment_name2",
                   // Default is "Invalid" since this should never be used when
                   // "ppm_survey_segment_max_memory_gb1" is 0.
                   "Invalid1");
BASE_FEATURE_PARAM(std::string,
                   kPerformanceControlsPPMSurveySegmentName3,
                   &kPerformanceControlsPPMSurvey,
                   "ppm_survey_segment_name3",
                   // Default is "Invalid" since this should never be used when
                   // "ppm_survey_segment_max_memory_gb1" is 0.
                   "Invalid2");
BASE_FEATURE_PARAM(size_t,
                   kPerformanceControlsPPMSurveySegmentMaxMemoryGB1,
                   &kPerformanceControlsPPMSurvey,
                   "ppm_survey_segment_max_memory_gb1",
                   0);
BASE_FEATURE_PARAM(size_t,
                   kPerformanceControlsPPMSurveySegmentMaxMemoryGB2,
                   &kPerformanceControlsPPMSurvey,
                   "ppm_survey_segment_max_memory_gb2",
                   0);

BASE_FEATURE(kPerformanceInterventionDemoMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPerformanceInterventionNotificationImprovements,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kMinimumTimeBetweenReshow,
                   &kPerformanceInterventionNotificationImprovements,
                   "minimum_time_reshow",
                   base::Hours(1));

BASE_FEATURE_PARAM(int,
                   kAcceptanceRateWindowSize,
                   &kPerformanceInterventionNotificationImprovements,
                   "window_size",
                   10);

BASE_FEATURE_PARAM(int,
                   kScaleMaxTimesPerDay,
                   &kPerformanceInterventionNotificationImprovements,
                   "scale_max_times_per_day",
                   3);

BASE_FEATURE_PARAM(int,
                   kScaleMaxTimesPerWeek,
                   &kPerformanceInterventionNotificationImprovements,
                   "scale_max_times_per_week",
                   21);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kNoAcceptanceBackOff,
                   &kPerformanceInterventionNotificationImprovements,
                   "no_acceptance_back_off",
                   base::Days(7));

BASE_FEATURE(kPerformanceInterventionNotificationStringImprovements,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kNotificationStringVersion,
                   &kPerformanceInterventionNotificationStringImprovements,
                   "string_version",
                   1);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kUnthrottledTabProcessReporting, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#endif

BASE_FEATURE(kEnableBestEffortTaskInhibitingPolicy,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBestEffortTaskInhibitingPeriod,
                   &kEnableBestEffortTaskInhibitingPolicy,
                   "enable_best_effort_task_inhibiting_period",
                   base::Minutes(5));
BASE_FEATURE_PARAM(
    base::TimeDelta,
    kBestEffortTaskInhibitingMinimumAllowedTimePerPeriod,
    &kEnableBestEffortTaskInhibitingPolicy,
    "enable_best_effort_task_inhibiting_minimum_allowed_time_per_period",
    base::Seconds(30));

BASE_FEATURE(kPMProcessPriorityPolicy, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kInheritParentPriority{
    &kPMProcessPriorityPolicy, "inherit_parent_priority", true};

const base::FeatureParam<bool> kRenderedOutOfViewIsNotVisible{
    &kPMProcessPriorityPolicy, "rendered_out_of_view_is_not_visible", false};

const base::FeatureParam<bool> kNonSpareRendererHighInitialPriority{
    &kPMProcessPriorityPolicy, "non_spare_renderer_high_initial_priority",
    false};

BASE_FEATURE(kPMLoadingPageVoter, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBFCachePerformanceManagerPolicy,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUrgentPageDiscarding, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCPUMeasurementInFreezingPolicy, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryMeasurementInFreezingPolicy,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscardFrozenBrowsingInstancesWithGrowingPMF,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Note: These params are associated with `kCPUMeasurementInFreezingPolicy`
// instead of `kFreezingOnBatterySaver` or
// `kDiscardFrozenBrowsingInstancesWithGrowingPMF`, to allow retrieving the
// value without activating these two features.
BASE_FEATURE_PARAM(int,
                   kFreezingMemoryGrowthThresholdToDiscardKb,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_memory_growth_threshold_to_discard_kb",
                   /* 100 MB */ 100 * 1024);

BASE_FEATURE_PARAM(double,
                   kFreezingHighCPUProportion,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_high_cpu_proportion",
                   0.05);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFreezingVisibleProtectionTime,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_visible_protection_time",
                   base::Minutes(5));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFreezingAudioProtectionTime,
                   &kCPUMeasurementInFreezingPolicy,
                   "freezing_audio_protection_time",
                   base::Minutes(5));

BASE_FEATURE(kFreezingOnBatterySaver, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFreezingOnBatterySaverForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFreezingFollowsDiscardOptOut, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRecordFreezingEligibilityUKM, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kInfiniteTabsFreezing, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kInfiniteTabsFreezing_NumProtectedTabs,
                   &kInfiniteTabsFreezing,
                   "num_protected_tabs",
                   5);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kInfiniteTabsFreezing_UnfreezeInterval,
                   &kInfiniteTabsFreezing,
                   "unfreeze_interval",
                   base::Minutes(1));

BASE_FEATURE_PARAM(base::TimeDelta,
                   kInfiniteTabsFreezing_UnfreezeDuration,
                   &kInfiniteTabsFreezing,
                   "unfreeze_duration",
                   base::Seconds(5));

BASE_FEATURE(kInfiniteTabsFreezingOnMemoryPressure,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kInfiniteTabsFreezingOnMemoryPressureInterval,
                   &kInfiniteTabsFreezingOnMemoryPressure,
                   "interval",
                   base::Seconds(5));

BASE_FEATURE_PARAM(int,
                   kInfiniteTabsFreezingOnMemoryPressurePercent,
                   &kInfiniteTabsFreezingOnMemoryPressure,
                   "percent_threshold",
                   15);

BASE_FEATURE(kResourceAttributionIncludeOrigins,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSeamlessRenderFrameSwap, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUnimportantFramesPriority, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThrottleUnimportantFrameRate, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKeepDefaultSearchEngineRendererAlive,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBoostClosingTabs, base::FEATURE_DISABLED_BY_DEFAULT);

// Defines the feature to enable this policy.
BASE_FEATURE(kTransientKeepAlivePolicy, base::FEATURE_DISABLED_BY_DEFAULT);

// Defines the Finch parameter for the keep-alive duration.
// Default is 23 seconds, which represents the threshold at which approximately
// 50% of renderer process could potentially be reused.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kTransientKeepAlivePolicyDuration,
                   &kTransientKeepAlivePolicy,
                   "duration",
                   base::Seconds(23));

// Defines the Finch parameter for the maximum number of empty renderer
// processes to keep alive simultaneously. Default is 10.
BASE_FEATURE_PARAM(size_t,
                   kTransientKeepAlivePolicyMaxCount,
                   &kTransientKeepAlivePolicy,
                   "count",
                   10);

BASE_FEATURE(kExtensionServiceWorkerVoter, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace performance_manager::features
