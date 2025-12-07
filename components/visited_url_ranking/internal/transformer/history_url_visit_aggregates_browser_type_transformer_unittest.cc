// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_browser_type_transformer.h"

#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using history::VisitContextAnnotations;

namespace {

struct TestParams {
  VisitContextAnnotations::BrowserType browser_type;
  size_t expected_count;
};

}  // namespace

namespace visited_url_ranking {

class HistoryURLVisitAggregatesBrowserTypeTransformerTest
    : public URLVisitAggregatesTransformerTest,
      public ::testing::WithParamInterface<TestParams> {
 public:
  HistoryURLVisitAggregatesBrowserTypeTransformerTest() = default;
  ~HistoryURLVisitAggregatesBrowserTypeTransformerTest() override = default;

  // Disallow copy/assign.
  HistoryURLVisitAggregatesBrowserTypeTransformerTest(
      const HistoryURLVisitAggregatesBrowserTypeTransformerTest&) = delete;
  HistoryURLVisitAggregatesBrowserTypeTransformerTest& operator=(
      const HistoryURLVisitAggregatesBrowserTypeTransformerTest&) = delete;

  void SetUp() override {
    transformer_ =
        std::make_unique<HistoryURLVisitAggregatesBrowserTypeTransformer>();
  }

  void TearDown() override { transformer_ = nullptr; }
};

TEST_P(HistoryURLVisitAggregatesBrowserTypeTransformerTest, Transform) {
  const auto& test_params = GetParam();
  history::AnnotatedVisit annotated_visit = GenerateSampleAnnotatedVisit(
      1, u"sample_title", GURL(kSampleSearchUrl), true, "foreign_session_guid",
      1.0f, {}, base::Time::Now(), test_params.browser_type);

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
    HistoryURLVisitAggregatesBrowserTypeTransformerTest,
    ::testing::Values(
        TestParams(VisitContextAnnotations::BrowserType::kTabbed, 1),
        TestParams(VisitContextAnnotations::BrowserType::kCustomTab, 1),
        TestParams(VisitContextAnnotations::BrowserType::kAuthTab, 0)));

}  // namespace visited_url_ranking
