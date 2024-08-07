// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_BROWSER_TYPE_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_BROWSER_TYPE_TRANSFORMER_H_

#include "base/functional/callback.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

class HistoryURLVisitAggregatesBrowserTypeTransformer
    : public URLVisitAggregatesTransformer {
 public:
  HistoryURLVisitAggregatesBrowserTypeTransformer() = default;
  ~HistoryURLVisitAggregatesBrowserTypeTransformer() override;

  // Disallow copy/assign.
  HistoryURLVisitAggregatesBrowserTypeTransformer(
      const HistoryURLVisitAggregatesBrowserTypeTransformer&) = delete;
  HistoryURLVisitAggregatesBrowserTypeTransformer& operator=(
      const HistoryURLVisitAggregatesBrowserTypeTransformer&) = delete;

  // URLVisitAggregatesTransformer:

  // Removes `URLVisitAggregate` objects whose visit annotations' context
  // annotation has browser type AuthView.
  void Transform(std::vector<URLVisitAggregate> aggregates,
                 const FetchOptions& options,
                 OnTransformCallback callback) override;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_BROWSER_TYPE_TRANSFORMER_H_
