// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_visibility_score_transformer.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct TestParams {
  float visibility_score;
  size_t expected_count;
};

}  // namespace

namespace visited_url_ranking {

constexpr float kSampleVisibiliytScoreThreshold = 0.7f;

class HistoryURLVisitAggregatesVisibilityScoreTransformerTest
    : public URLVisitAggregatesTransformerTest,
      public ::testing::WithParamInterface<TestParams> {
 public:
  HistoryURLVisitAggregatesVisibilityScoreTransformerTest() = default;
  ~HistoryURLVisitAggregatesVisibilityScoreTransformerTest() override = default;

  // Disallow copy/assign.
  HistoryURLVisitAggregatesVisibilityScoreTransformerTest(
      const HistoryURLVisitAggregatesVisibilityScoreTransformerTest&) = delete;
  HistoryURLVisitAggregatesVisibilityScoreTransformerTest& operator=(
      const HistoryURLVisitAggregatesVisibilityScoreTransformerTest&) = delete;

  void SetUp() override {
    transformer_ =
        std::make_unique<HistoryURLVisitAggregatesVisibilityScoreTransformer>(
            kSampleVisibiliytScoreThreshold);
  }

  void TearDown() override { transformer_ = nullptr; }
};

TEST_P(HistoryURLVisitAggregatesVisibilityScoreTransformerTest, Transform) {
  const TestParams& test_params = GetParam();
  history::AnnotatedVisit annotated_visit =
      GenerateSampleAnnotatedVisit(1, u"sample_title", GURL(kSampleSearchUrl),
                                   true, "", test_params.visibility_score);
  URLVisitAggregate visit_aggregate(kSampleSearchUrl);
  visit_aggregate.fetcher_data_map.emplace(
      Fetcher::kHistory,
      URLVisitAggregate::HistoryData(std::move(annotated_visit)));
  std::vector<URLVisitAggregate> input_sample_aggregates = {};
  input_sample_aggregates.push_back(std::move(visit_aggregate));

  URLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(input_sample_aggregates));
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), test_params.expected_count);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HistoryURLVisitAggregatesVisibilityScoreTransformerTest,
    ::testing::Values(
        TestParams(/*visiblity_score=*/1.0f, /*expected_count=*/1),
        TestParams(/*visiblity_score=*/0.5f, /*expected_count=*/0)));

}  // namespace visited_url_ranking
