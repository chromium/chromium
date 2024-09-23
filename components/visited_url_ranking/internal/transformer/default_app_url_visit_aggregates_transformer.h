// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_DEFAULT_APP_URL_VISIT_AGGREGATES_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_DEFAULT_APP_URL_VISIT_AGGREGATES_TRANSFORMER_H_

#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

// A transformer to remove URLs if they can be opened by default apps.
class DefaultAppURLVisitAggregatesTransformer
    : public URLVisitAggregatesTransformer {
 public:
  explicit DefaultAppURLVisitAggregatesTransformer(
      base::flat_set<std::string_view> default_app_blocklist);

  ~DefaultAppURLVisitAggregatesTransformer() override;

  // Disallow copy/assign.
  DefaultAppURLVisitAggregatesTransformer(
      const DefaultAppURLVisitAggregatesTransformer&) = delete;
  DefaultAppURLVisitAggregatesTransformer& operator=(
      const DefaultAppURLVisitAggregatesTransformer&) = delete;

  // URLVisitAggregatesTransformer:

  // Removes `URLVisitAggregate` objects that have whose visit annotations
  // categories overlap with the `default_app_blocklist` set.
  void Transform(std::vector<URLVisitAggregate> aggregates,
                 const FetchOptions& options,
                 OnTransformCallback callback) override;

 private:
  base::flat_set<std::string_view> default_app_blocklist_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_DEFAULT_APP_URL_VISIT_AGGREGATES_TRANSFORMER_H_
