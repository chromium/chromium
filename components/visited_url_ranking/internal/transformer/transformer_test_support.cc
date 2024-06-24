// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

URLVisitAggregatesTransformerTest::URLVisitAggregatesTransformerTest() =
    default;

URLVisitAggregatesTransformerTest::~URLVisitAggregatesTransformerTest() =
    default;

URLVisitAggregatesTransformerTest::Result
URLVisitAggregatesTransformerTest::TransformAndGetResult(
    std::vector<URLVisitAggregate> aggregates) {
  return TransformAndGetResult(
      std::move(aggregates),
      FetchOptions::CreateDefaultFetchOptionsForTabResumption());
}

URLVisitAggregatesTransformerTest::Result
URLVisitAggregatesTransformerTest::TransformAndGetResult(
    std::vector<URLVisitAggregate> aggregates,
    const FetchOptions& options) {
  base::RunLoop wait_loop;
  URLVisitAggregatesTransformerTest::Result result;
  transformer_->Transform(std::move(aggregates), options,
                          base::BindOnce(
                              [](base::OnceClosure stop_waiting, Result* result,
                                 URLVisitAggregatesTransformer::Status status,
                                 std::vector<URLVisitAggregate> aggregates) {
                                result->first = status;
                                result->second = std::move(aggregates);
                                std::move(stop_waiting).Run();
                              },
                              wait_loop.QuitClosure(), &result));
  wait_loop.Run();
  return result;
}

}  // namespace visited_url_ranking
