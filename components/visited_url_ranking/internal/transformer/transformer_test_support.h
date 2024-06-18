// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_TRANSFORMER_TEST_SUPPORT_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_TRANSFORMER_TEST_SUPPORT_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

class URLVisitAggregatesTransformerTest : public testing::Test {
 public:
  URLVisitAggregatesTransformerTest();
  ~URLVisitAggregatesTransformerTest() override;

  // Disallow copy/assign.
  URLVisitAggregatesTransformerTest(const URLVisitAggregatesTransformerTest&) =
      delete;
  URLVisitAggregatesTransformerTest& operator=(
      const URLVisitAggregatesTransformerTest&) = delete;

  using Result = std::pair<URLVisitAggregatesTransformer::Status,
                           std::vector<URLVisitAggregate>>;
  Result TransformAndGetResult(std::vector<URLVisitAggregate> aggregates);
  Result TransformAndGetResult(std::vector<URLVisitAggregate> aggregates,
                               const FetchOptions& options);

 protected:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<URLVisitAggregatesTransformer> transformer_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_TRANSFORMER_TEST_SUPPORT_H_
