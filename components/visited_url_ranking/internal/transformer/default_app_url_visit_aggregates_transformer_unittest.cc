// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/default_app_url_visit_aggregates_transformer.h"

#include <iterator>
#include <memory>
#include <string_view>
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
#include "components/visited_url_ranking/public/url_visit_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace visited_url_ranking {

constexpr char kSampleUrl[] = "https://www.foo.com";
constexpr char kUrlForDefaultApp[] = "https://docs.google.com/presentation/u/0";

class DefaultAppURLVisitAggregatesTransformerTest
    : public URLVisitAggregatesTransformerTest {
 public:
  DefaultAppURLVisitAggregatesTransformerTest() = default;

  // Disallow copy/assign.
  DefaultAppURLVisitAggregatesTransformerTest(
      const DefaultAppURLVisitAggregatesTransformerTest&) = delete;
  DefaultAppURLVisitAggregatesTransformerTest& operator=(
      const DefaultAppURLVisitAggregatesTransformerTest&) = delete;

  void SetUp() override {
    base::flat_set<std::string_view> default_app_blocklist(
        kDefaultAppBlocklist.begin(), kDefaultAppBlocklist.end());
    transformer_ = std::make_unique<DefaultAppURLVisitAggregatesTransformer>(
        std::move(default_app_blocklist));
  }

  void TearDown() override { transformer_ = nullptr; }
};

TEST_F(DefaultAppURLVisitAggregatesTransformerTest, Transform) {
  std::vector<URLVisitAggregate> input_sample_aggregates = {};
  input_sample_aggregates.push_back(
      CreateSampleURLVisitAggregate(GURL(kSampleUrl)));

  DefaultAppURLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(input_sample_aggregates));
  size_t expected_count = 1;
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), expected_count);
}

TEST_F(DefaultAppURLVisitAggregatesTransformerTest, TransformRemoveUrl) {
  std::vector<URLVisitAggregate> input_sample_aggregates = {};
  input_sample_aggregates.push_back(
      CreateSampleURLVisitAggregate(GURL(kUrlForDefaultApp)));

  DefaultAppURLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(input_sample_aggregates));
  size_t expected_count = 0;
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), expected_count);
}

}  // namespace visited_url_ranking
