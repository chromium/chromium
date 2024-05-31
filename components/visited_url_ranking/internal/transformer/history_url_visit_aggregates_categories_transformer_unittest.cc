// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_categories_transformer.h"

#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/test/mock_callback.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct TestParams {
  std::vector<std::string> category_ids;
  size_t expected_count;
};

}  // namespace

namespace visited_url_ranking {

constexpr std::string kSampleBlocklistedCategoryId = "travel";

class HistoryURLVisitAggregatesCategoriesTransformerTest
    : public URLVisitAggregatesTransformerTest,
      public ::testing::WithParamInterface<TestParams> {
 public:
  HistoryURLVisitAggregatesCategoriesTransformerTest() = default;
  ~HistoryURLVisitAggregatesCategoriesTransformerTest() override = default;

  // Disallow copy/assign.
  HistoryURLVisitAggregatesCategoriesTransformerTest(
      const HistoryURLVisitAggregatesCategoriesTransformerTest&) = delete;
  HistoryURLVisitAggregatesCategoriesTransformerTest& operator=(
      const HistoryURLVisitAggregatesCategoriesTransformerTest&) = delete;

  void SetUp() override {
    transformer_ =
        std::make_unique<HistoryURLVisitAggregatesCategoriesTransformer>(
            categories_blocklist_);
  }

  void TearDown() override { transformer_ = nullptr; }

 private:
  const base::flat_set<std::string> categories_blocklist_ = {
      kSampleBlocklistedCategoryId};
};

TEST_P(HistoryURLVisitAggregatesCategoriesTransformerTest, Transform) {
  std::vector<history::VisitContentModelAnnotations::Category> categories;
  const auto& test_params = GetParam();
  std::transform(test_params.category_ids.begin(),
                 test_params.category_ids.end(), std::back_inserter(categories),
                 [](std::string category_id) {
                   return history::VisitContentModelAnnotations::Category(
                       category_id, 100);
                 });
  history::AnnotatedVisit annotated_visit = GenerateSampleAnnotatedVisit(
      1, u"sample_title", GURL(kSampleSearchUrl), true, "foreign_session_guid",
      1.0f, std::move(categories));

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

constexpr std::vector<std::string> kSampleEmptyCategoryIds = {};
INSTANTIATE_TEST_SUITE_P(
    All,
    HistoryURLVisitAggregatesCategoriesTransformerTest,
    ::testing::Values(
        TestParams(/*category_ids=*/kSampleEmptyCategoryIds,
                   /*expected_count=*/1),
        TestParams(/*category_ids=*/{kSampleBlocklistedCategoryId},
                   /*expected_count=*/0)));

}  // namespace visited_url_ranking
