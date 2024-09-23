// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_AGGREGATES_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_AGGREGATES_TRANSFORMER_H_

#include <map>
#include <vector>

#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

// Derived classes implement logic responsible for modifying a collection of
// `URLVisitAggregate` objects.
class URLVisitAggregatesTransformer {
 public:
  enum class Status { kError = 0, kSuccess = 1 };
  virtual ~URLVisitAggregatesTransformer() = default;

  using OnTransformCallback =
      base::OnceCallback<void(Status status, std::vector<URLVisitAggregate>)>;
  virtual void Transform(std::vector<URLVisitAggregate> aggregates,
                         const FetchOptions& options,
                         OnTransformCallback callback) = 0;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_AGGREGATES_TRANSFORMER_H_
