// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_CATEGORIES_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_CATEGORIES_TRANSFORMER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

class HistoryURLVisitAggregatesCategoriesTransformer
    : public URLVisitAggregatesTransformer {
 public:
  explicit HistoryURLVisitAggregatesCategoriesTransformer(
      base::flat_set<std::string> blocklisted_categories);
  ~HistoryURLVisitAggregatesCategoriesTransformer() override;

  // Disallow copy/assign.
  HistoryURLVisitAggregatesCategoriesTransformer(
      const HistoryURLVisitAggregatesCategoriesTransformer&) = delete;
  HistoryURLVisitAggregatesCategoriesTransformer& operator=(
      const HistoryURLVisitAggregatesCategoriesTransformer&) = delete;

  // URLVisitAggregatesTransformer:

  // Removes `URLVisitAggregate` objects that have whose visit annotations
  // categories overlap with the `blocklisted_categories` set.
  void Transform(std::vector<URLVisitAggregate> aggregates,
                 const FetchOptions& options,
                 OnTransformCallback callback) override;

 private:
  base::flat_set<std::string> blocklisted_categories_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_HISTORY_URL_VISIT_AGGREGATES_CATEGORIES_TRANSFORMER_H_
