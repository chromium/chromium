// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_RECENCY_FILTER_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_RECENCY_FILTER_TRANSFORMER_H_

#include "components/visited_url_ranking/internal/transformer/bookmarks_url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

// Transformer that removes old entries, and trims long tails. This cannot be
// done in fetcher since we still want signals about the URLs for longer period.
class RecencyFilterTransformer : public URLVisitAggregatesTransformer {
 public:
  RecencyFilterTransformer();
  ~RecencyFilterTransformer() override;

  RecencyFilterTransformer(const RecencyFilterTransformer&) = delete;
  RecencyFilterTransformer& operator=(const RecencyFilterTransformer&) = delete;

  // URLVisitAggregatesTransformer:
  void Transform(std::vector<URLVisitAggregate> aggregates,
                 const FetchOptions& options,
                 OnTransformCallback callback) override;

 private:
  const size_t aggregate_count_limit_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_RECENCY_FILTER_TRANSFORMER_H_
