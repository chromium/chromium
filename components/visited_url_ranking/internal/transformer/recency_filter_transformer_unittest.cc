// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/recency_filter_transformer.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

namespace {

constexpr char kTestUrl1[] = "https://www.example1.com/";
constexpr base::TimeDelta kTimeSinceLoad1 = base::Hours(20);
constexpr char kTestUrl2[] = "https://www.example2.com/";
constexpr base::TimeDelta kTimeSinceLoad2 = base::Hours(40);
constexpr char kTestUrl3[] = "https://www.example3.com/";
constexpr base::TimeDelta kTimeSinceLoad3 = base::Hours(60);
constexpr char kTestUrl4[] = "https://www.example4.com/";
constexpr base::TimeDelta kTimeSinceLoad4 = base::Hours(80);
constexpr char kTestUrl5[] = "https://www.example5.com/";
constexpr base::TimeDelta kTimeSinceLoad5 = base::Hours(100);
constexpr char kTestUrl6[] = "https://www.example6.com/";
constexpr base::TimeDelta kTimeSinceLoad6 = base::Hours(120);
constexpr char kTestUrl7[] = "https://www.example7.com/";
constexpr base::TimeDelta kTimeSinceLoad7 = base::Hours(140);
constexpr char kTestUrl8[] = "https://www.example8.com/";
constexpr base::TimeDelta kTimeSinceLoad8 = base::Hours(160);
constexpr char kTestUrl9[] = "https://www.example9.com/";
constexpr base::TimeDelta kTimeSinceLoad9 = base::Hours(180);
constexpr char kTestUrl10[] = "https://www.example10.com/";
constexpr base::TimeDelta kTimeSinceLoad10 = base::Hours(200);

constexpr unsigned kTestCountLimit = 5;

class RecencyFilterTransformerTest : public URLVisitAggregatesTransformerTest {
 public:
  RecencyFilterTransformerTest() = default;
  ~RecencyFilterTransformerTest() override = default;

  void SetUp() override {
    Test::SetUp();

    base::FieldTrialParams params = {
        {features::kURLAggregateCountLimit,
         base::StringPrintf("%u", kTestCountLimit)}};
    base::test::ScopedFeatureList feature_list1;
    feature_list1.InitWithFeaturesAndParameters(
        {{features::kVisitedURLRankingService, params}}, {});
    transformer_ = std::make_unique<RecencyFilterTransformer>();
  }

  void TearDown() override {
    transformer_.reset();
    Test::TearDown();
  }
};

TEST_F(RecencyFilterTransformerTest, SortAndFilter) {
  base::Time now = base::Time::Now();

  std::vector<URLVisitAggregate> candidates = {};

  // Insert candidates not sorted by timestamp:
  {
    // Entries with only tabs:
    candidates.push_back(CreateSampleURLVisitAggregate(
        GURL(kTestUrl9), 1, now - kTimeSinceLoad9, {Fetcher::kTabModel}));
    candidates.push_back(CreateSampleURLVisitAggregate(
        GURL(kTestUrl3), 1, now - kTimeSinceLoad3, {Fetcher::kTabModel}));

    // Entries with only history, both are older than 24 hours, and should be
    // filtered out.
    candidates.push_back(CreateSampleURLVisitAggregate(
        GURL(kTestUrl10), 1, now - kTimeSinceLoad10, {Fetcher::kHistory}));
    candidates.push_back(CreateSampleURLVisitAggregate(
        GURL(kTestUrl4), 1, now - kTimeSinceLoad4, {Fetcher::kHistory}));

    // Entries with both:
    candidates.push_back(CreateSampleURLVisitAggregate(GURL(kTestUrl8), 1,
                                                       now - kTimeSinceLoad8));
    candidates.push_back(CreateSampleURLVisitAggregate(GURL(kTestUrl1), 1,
                                                       now - kTimeSinceLoad1));
    candidates.push_back(CreateSampleURLVisitAggregate(GURL(kTestUrl2), 1,
                                                       now - kTimeSinceLoad2));
    candidates.push_back(CreateSampleURLVisitAggregate(GURL(kTestUrl6), 1,
                                                       now - kTimeSinceLoad6));
    candidates.push_back(CreateSampleURLVisitAggregate(GURL(kTestUrl7), 1,
                                                       now - kTimeSinceLoad7));
    candidates.push_back(CreateSampleURLVisitAggregate(GURL(kTestUrl5), 1,
                                                       now - kTimeSinceLoad5));
  }

  URLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(candidates));

  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), kTestCountLimit);

  EXPECT_EQ(*(*result.second[0].GetAssociatedURLs().begin()), GURL(kTestUrl1));
  EXPECT_EQ(*(*result.second[1].GetAssociatedURLs().begin()), GURL(kTestUrl2));
  EXPECT_EQ(*(*result.second[2].GetAssociatedURLs().begin()), GURL(kTestUrl3));
  EXPECT_EQ(*(*result.second[3].GetAssociatedURLs().begin()), GURL(kTestUrl5));
  EXPECT_EQ(*(*result.second[4].GetAssociatedURLs().begin()), GURL(kTestUrl6));
}

}  // namespace
}  // namespace visited_url_ranking
