// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace visited_url_ranking::features {

// Core feature flag for Visited URL Ranking service.
BASE_DECLARE_FEATURE(kVisitedURLRankingService);

// Parameter determining the fetch option's default query duration in hours.
extern const char kVisitedURLRankingFetchDurationInHoursParam[];

// Parameter to determine the age limit for history entries to be ranked.
extern const char kHistoryAgeThresholdHours[];
extern const int kHistoryAgeThresholdHoursDefaultValue;

// Parameter to determine the age limit for tab entries to be ranked.
extern const char kTabAgeThresholdHours[];
extern const int kTabAgeThresholdHoursDefaultValue;

// Max number of URL aggregate candidates to rank.
extern const char kURLAggregateCountLimit[];
extern const int kURLAggregateCountLimitDefaultValue;

// Feature flag to disable History visibility score filter.
BASE_DECLARE_FEATURE(kVisitedURLRankingHistoryVisibilityScoreFilter);

// Feature flag to disable the segmentation metrics transformer.
BASE_DECLARE_FEATURE(kVisitedURLRankingSegmentationMetricsData);

}  // namespace visited_url_ranking::features

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_
