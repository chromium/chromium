// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/recency_filter_transformer.h"

#include <algorithm>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

RecencyFilterTransformer::RecencyFilterTransformer()
    : history_age_threshold_(base::Hours(base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService,
          features::kHistoryAgeThresholdHours,
          features::kHistoryAgeThresholdHoursDefaultValue))),
      tab_age_threshold_(base::Hours(base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService,
          features::kTabAgeThresholdHours,
          features::kTabAgeThresholdHoursDefaultValue))),
      aggregate_count_limit_(base::GetFieldTrialParamByFeatureAsInt(
          features::kVisitedURLRankingService,
          features::kURLAggregateCountLimit,
          features::kURLAggregateCountLimitDefaultValue)) {}

RecencyFilterTransformer::~RecencyFilterTransformer() = default;

void RecencyFilterTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    OnTransformCallback callback) {
  std::erase_if(aggregates, [&](const auto& url_visit_aggregate) {
    base::TimeDelta age_limit;
    if (url_visit_aggregate.fetcher_data_map.count(Fetcher::kTabModel) ||
        url_visit_aggregate.fetcher_data_map.count(Fetcher::kSession)) {
      age_limit = tab_age_threshold_;
    } else {
      DCHECK(url_visit_aggregate.fetcher_data_map.count(Fetcher::kHistory));
      age_limit = history_age_threshold_;
    }
    return base::Time::Now() - url_visit_aggregate.GetLastVisitTime() >
           age_limit;
  });

  std::sort(aggregates.begin(), aggregates.end(),
            [](const URLVisitAggregate& a, const URLVisitAggregate& b) {
              return a.GetLastVisitTime() > b.GetLastVisitTime();
            });
  if (aggregates.size() > aggregate_count_limit_) {
    std::vector<URLVisitAggregate> copy;
    for (size_t i = 0; i < aggregate_count_limit_; ++i) {
      copy.emplace_back(std::move(aggregates[i]));
    }
    aggregates.swap(copy);
  }

  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking
