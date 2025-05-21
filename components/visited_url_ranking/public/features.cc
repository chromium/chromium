// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace visited_url_ranking::features {

BASE_FEATURE(kVisitedURLRankingService,
             "VisitedURLRankingService",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool>
    kVisitedURLRankingHistoryFetcherDiscardZeroDurationVisits{
        &kVisitedURLRankingService,
        /*name=*/"history_fetcher_discard_zero_duration_visits",
#if BUILDFLAG(IS_ANDROID)
        /*default_value=*/false};
#else
        /*default_value=*/true};
#endif  // BUILDFLAG(IS_ANDROID)

constexpr base::FeatureParam<std::string> kVisitedURLRankingResultTypesParam{
    &kVisitedURLRankingService,
    /*name=*/"visited_url_ranking_url_types",
    /*default_value=*/""};

constexpr base::FeatureParam<bool> kVisitedURLRankingRecordActions{
    &kVisitedURLRankingService,
    /*name=*/"visited_url_ranking_record_actions",
    /*default_value=*/false};

const char kVisitedURLRankingFetchDurationInHoursParam[] =
    "VisitedURLRankingFetchDurationInHoursParam";

const char kHistoryAgeThresholdHours[] = "history_age_threshold_hours";
// 1 day in hours.
const int kHistoryAgeThresholdHoursDefaultValue = 24;

const char kTabAgeThresholdHours[] = "tab_age_threshold_hours";
// 7 days in hours.
const int kTabAgeThresholdHoursDefaultValue = 168;

const char kURLAggregateCountLimit[] = "aggregate_count_limit";
const int kURLAggregateCountLimitDefaultValue = 50;

BASE_FEATURE(kVisitedURLRankingHistoryVisibilityScoreFilter,
             "VisitedURLRankingHistoryVisibilityScoreFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kVisitedURLRankingSegmentationMetricsData,
             "VisitedURLRankingSegmentationMetricsData",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kVisitedURLRankingDeduplication,
             "VisitedURLRankingDeduplication",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kVisitedURLRankingDeduplicationDocs{
    &kVisitedURLRankingDeduplication, /*name=*/"url_deduplication_docs",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kVisitedURLRankingDeduplicationSearchEngine{
    &kVisitedURLRankingDeduplication,
    /*name=*/"url_deduplication_search_engine",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kVisitedURLRankingDeduplicationFallback{
    &kVisitedURLRankingDeduplication, /*name=*/"url_deduplication_fallback",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kVisitedURLRankingDeduplicationUpdateScheme{
    &kVisitedURLRankingDeduplication,
    /*name=*/"url_deduplication_update_scheme",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kVisitedURLRankingDeduplicationClearPath{
    &kVisitedURLRankingDeduplication,
    /*name=*/"url_deduplication_clear_path",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kVisitedURLRankingDeduplicationIncludeTitle{
    &kVisitedURLRankingDeduplication,
    /*name=*/"url_deduplication_include_title",
    /*default_value=*/true};

constexpr base::FeatureParam<std::string>
    kVisitedURLRankingDeduplicationExcludedPrefixes{
        &kVisitedURLRankingDeduplication,
        /*name=*/"url_deduplication_excluded_prefixes",
        /*default_value=*/"www.; login.corp.; myaccount.; accounts.;"};

BASE_FEATURE(kVisitedURLRankingDecorations,
             "VisitedURLRankingDecorations",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kVisitedURLRankingDecorationTimeOfDay{
    &kVisitedURLRankingDecorations,
    /*name=*/"decorations_time_of_day_threshold",
    /*default_value=*/5};

constexpr base::FeatureParam<int> kVisitedURLRankingFrequentlyVisitedThreshold{
    &kVisitedURLRankingDecorations,
    /*name=*/"decorations_frequently_visited_threshold",
    /*default_value=*/5};

constexpr base::FeatureParam<int>
    kVisitedURLRankingDecorationRecentlyVisitedMinutesThreshold{
        &kVisitedURLRankingDecorations,
        /*name=*/"decorations_recently_visited_minutes_threshold",
        /*default_value=*/1};

BASE_FEATURE(kVisitedURLRankingScoreThreshold,
             "VisitedURLRankingScoreThreshold",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<double>
    kVisitedURLRankingScoreThresholdActiveLocalTab{
        &kVisitedURLRankingScoreThreshold,
        /*name=*/"active_local_tab_score_threshold",
        /*default_value=*/0};

constexpr base::FeatureParam<double>
    kVisitedURLRankingScoreThresholdActiveRemoteTab{
        &kVisitedURLRankingScoreThreshold,
        /*name=*/"active_remote_tab_score_threshold",
        /*default_value=*/0};

constexpr base::FeatureParam<double> kVisitedURLRankingScoreThresholdLocalVisit{
    &kVisitedURLRankingScoreThreshold,
    /*name=*/"local_visit_score_threshold",
    /*default_value=*/0};

constexpr base::FeatureParam<double>
    kVisitedURLRankingScoreThresholdRemoteVisit{
        &kVisitedURLRankingScoreThreshold,
        /*name=*/"remote_visit_score_threshold",
        /*default_value=*/0};

constexpr base::FeatureParam<double> kVisitedURLRankingScoreThresholdCCTVisit{
    &kVisitedURLRankingScoreThreshold,
    /*name=*/"cct_visit_score_threshold",
    /*default_value=*/0};

BASE_FEATURE(kGroupSuggestionService,
             "GroupSuggestionService",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kGroupSuggestionEnableRecentlyOpened{
    &kGroupSuggestionService,
    /*name=*/"group_suggestion_enable_recently_opened",
    /*default_value=*/false};

constexpr base::FeatureParam<bool> kGroupSuggestionEnableSwitchBetween{
    &kGroupSuggestionService,
    /*name=*/"group_suggestion_enable_switch_between",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kGroupSuggestionEnableSimilarSource{
    &kGroupSuggestionService,
    /*name=*/"group_suggestion_enable_similar_source",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kGroupSuggestionEnableSameOrigin{
    &kGroupSuggestionService,
    /*name=*/"group_suggestion_enable_same_origin",
    /*default_value=*/false};

constexpr base::FeatureParam<bool> kGroupSuggestionEnableTabSwitcherOnly{
    &kGroupSuggestionService,
    /*name=*/"group_suggestion_enable_tab_switcher_only",
    /*default_value=*/false};

constexpr base::FeatureParam<bool> kGroupSuggestionEnableVisibilityCheck{
    &kGroupSuggestionService,
    /*name=*/"group_suggestion_enable_visibility_check",
    /*default_value=*/true};

constexpr base::FeatureParam<bool> kGroupSuggestionTriggerCalculationOnPageLoad{
    &kGroupSuggestionService,
    /*name=*/"group_suggestion_trigger_calculation_on_page_load",
    /*default_value=*/true};

constexpr base::FeatureParam<base::TimeDelta> kGroupSuggestionThrottleAgeLimit{
    &kGroupSuggestionService,
    /*name=*/"group_suggestion_throttle_age_limit",
    /*default_value=*/base::Days(1)};
}  // namespace visited_url_ranking::features
