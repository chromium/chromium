// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/tab_events_visit_transformer.h"

#include <memory>

#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

namespace {

constexpr char kTestUrl1[] = "https://www.example1.com/";
constexpr base::TimeDelta kTimeSinceLoad1 = base::Hours(20);
constexpr char kTestUrl2[] = "https://www.example2.com/";
constexpr base::TimeDelta kTimeSinceLoad2 = base::Hours(40);

class TabEventsVisitTransformerTest : public URLVisitAggregatesTransformerTest {
 public:
  TabEventsVisitTransformerTest() = default;
  ~TabEventsVisitTransformerTest() override = default;

  void SetUp() override {
    Test::SetUp();
    transformer_ = std::make_unique<TabEventsVisitTransformer>();
  }

  void TearDown() override {
    transformer_.reset();
    Test::TearDown();
  }

  std::vector<URLVisitAggregate> GetSampleCandidates() {
    base::Time now = base::Time::Now();
    std::vector<URLVisitAggregate> candidates = {};

    // Entries with only tabs:
    candidates.push_back(CreateSampleURLVisitAggregate(
        GURL(kTestUrl1), 1, now - kTimeSinceLoad1, {Fetcher::kTabModel}));
    candidates.push_back(CreateSampleURLVisitAggregate(
        GURL(kTestUrl2), 2, now - kTimeSinceLoad2, {Fetcher::kTabModel}));

    return candidates;
  }
};

TEST_F(TabEventsVisitTransformerTest, Transform) {
  auto candidates = GetSampleCandidates();
  URLVisitAggregatesTransformerTest::Result result = TransformAndGetResult(
      std::move(candidates),
      FetchOptions::CreateDefaultFetchOptionsForTabResumption());
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  EXPECT_EQ(result.second.size(), 2u);
}

}  // namespace

}  // namespace visited_url_ranking
