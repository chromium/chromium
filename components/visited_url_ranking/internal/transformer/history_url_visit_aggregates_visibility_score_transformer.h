// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_VISIBILITY_SCORE_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_VISIBILITY_SCORE_TRANSFORMER_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

class HistoryURLVisitAggregatesVisibilityScoreTransformer
    : public URLVisitAggregatesTransformer {
 public:
  explicit HistoryURLVisitAggregatesVisibilityScoreTransformer(
      float visibility_score);
  ~HistoryURLVisitAggregatesVisibilityScoreTransformer() override = default;

  // Disallow copy/assign.
  HistoryURLVisitAggregatesVisibilityScoreTransformer(
      const HistoryURLVisitAggregatesVisibilityScoreTransformer&) = delete;
  HistoryURLVisitAggregatesVisibilityScoreTransformer& operator=(
      const HistoryURLVisitAggregatesVisibilityScoreTransformer&) = delete;

  // URLVisitAggregatesTransformer:

  // Removes `URLVisitAggregate` objects who visibility score does not satisfy
  // the `visibility_score_threshold_` value.
  void Transform(std::vector<URLVisitAggregate> aggregates,
                 const FetchOptions& options,
                 OnTransformCallback callback) override;

 private:
  const float visibility_score_threshold_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_VISIBILITY_SCORE_TRANSFORMER_H_
