// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/public/features.h"

#include "build/build_config.h"

namespace visited_url_ranking::features {

BASE_FEATURE(kVisitedURLRankingService,
             "VisitedURLRankingService",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

}  // namespace visited_url_ranking::features
