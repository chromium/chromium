// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/recency_filter_transformer.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit.h"
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
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kVisitedURLRankingService, params}}, {});
    transformer_ = std::make_unique<RecencyFilterTransformer>();
  }

  void TearDown() override {
    transformer_.reset();
    Test::TearDown();
  }

  std::vector<URLVisitAggregate> GetSampleCandidates() {
    base::Time now = base::Time::Now();
    std::vector<URLVisitAggregate> candidates = {};

    // Insert candidates not sorted by timestamp, in random order to test if
    // most recent ones are retained.

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

    // Entry with only remote tab.
    candidates.push_back(CreateSampleURLVisitAggregate(
        GURL(kTestUrl5), 1, now - kTimeSinceLoad5, {Fetcher::kSession}));

    // Entries with all types:
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
    return candidates;
  }

  URLVisitAggregate::TabData CreateSampleTabData(const std::u16string& title,
                                                 const URLVisit::Source& source,
                                                 const base::Time& time) {
    std::optional<std::string> sample_tag = std::nullopt;
    std::optional<std::string> sample_session_name = std::nullopt;
    if (source == URLVisit::Source::kForeign) {
      sample_tag = "sample_tag";
      sample_session_name = "sample_session_name";
    }
    auto tab_data = URLVisitAggregate::TabData(URLVisitAggregate::Tab(
        1,
        URLVisit(GURL(kSampleSearchUrl), title, time,
                 syncer::DeviceInfo::FormFactor::kDesktop, source),
        std::move(sample_tag), std::move(sample_session_name)));
    tab_data.last_active = time;
    return tab_data;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RecencyFilterTransformerTest, SortAndFilter) {
  auto candidates = GetSampleCandidates();
  FetchOptions fetch_options = FetchOptions::CreateFetchOptionsForTabResumption(
      URLVisitAggregate::kAllResultTypes);
  URLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(candidates), fetch_options);

  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), kTestCountLimit);

  // 4 is removed because it is only history and is older than history age 1
  // day.
  EXPECT_EQ(*(*result.second[0].GetAssociatedURLs().begin()), GURL(kTestUrl1));
  EXPECT_EQ(*(*result.second[1].GetAssociatedURLs().begin()), GURL(kTestUrl2));
  EXPECT_EQ(*(*result.second[2].GetAssociatedURLs().begin()), GURL(kTestUrl3));
  EXPECT_EQ(*(*result.second[3].GetAssociatedURLs().begin()), GURL(kTestUrl5));
  EXPECT_EQ(*(*result.second[4].GetAssociatedURLs().begin()), GURL(kTestUrl6));
}

TEST_F(RecencyFilterTransformerTest, FilteringTabs) {
  auto candidates = GetSampleCandidates();
  FetchOptions fetch_options = FetchOptions::CreateFetchOptionsForTabResumption(
      {URLVisitAggregate::URLType::kActiveLocalTab});
  URLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(candidates), fetch_options);
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), kTestCountLimit);

  // 4 and 5 are removed because they do not have local tab.
  EXPECT_EQ(*(*result.second[0].GetAssociatedURLs().begin()), GURL(kTestUrl1));
  EXPECT_EQ(*(*result.second[1].GetAssociatedURLs().begin()), GURL(kTestUrl2));
  EXPECT_EQ(*(*result.second[2].GetAssociatedURLs().begin()), GURL(kTestUrl3));
  EXPECT_EQ(*(*result.second[3].GetAssociatedURLs().begin()), GURL(kTestUrl6));
  EXPECT_EQ(*(*result.second[4].GetAssociatedURLs().begin()), GURL(kTestUrl7));
}

TEST_F(RecencyFilterTransformerTest, FilteringHistory) {
  auto candidates = GetSampleCandidates();
  FetchOptions fetch_options = FetchOptions::CreateFetchOptionsForTabResumption(
      {URLVisitAggregate::URLType::kRemoteVisit});
  URLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(candidates), fetch_options);
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), 1u);

  // Keeps only 1 since all other entries are over history age limit - 1 day.
  EXPECT_EQ(*(*result.second[0].GetAssociatedURLs().begin()), GURL(kTestUrl1));
}

TEST_F(RecencyFilterTransformerTest, FilteringRemoteTab) {
  auto candidates = GetSampleCandidates();
  FetchOptions fetch_options = FetchOptions::CreateFetchOptionsForTabResumption(
      {URLVisitAggregate::URLType::kActiveRemoteTab});
  URLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(candidates), fetch_options);
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), 5u);

  // 3 and 4 are removed because they do not have remote tab.
  EXPECT_EQ(*(*result.second[0].GetAssociatedURLs().begin()), GURL(kTestUrl1));
  EXPECT_EQ(*(*result.second[1].GetAssociatedURLs().begin()), GURL(kTestUrl2));
  EXPECT_EQ(*(*result.second[2].GetAssociatedURLs().begin()), GURL(kTestUrl5));
  EXPECT_EQ(*(*result.second[3].GetAssociatedURLs().begin()), GURL(kTestUrl6));
  EXPECT_EQ(*(*result.second[4].GetAssociatedURLs().begin()), GURL(kTestUrl7));
}

TEST_F(RecencyFilterTransformerTest, FilterVisitAggregatesWithLocalTabURLType) {
  base::Time now = base::Time::Now();

  URLVisitAggregate visit_aggregate_with_local_tab_data(kSampleSearchUrl);
  visit_aggregate_with_local_tab_data.fetcher_data_map.emplace(
      Fetcher::kTabModel,
      CreateSampleTabData(u"sample_title", URLVisit::Source::kLocal, now));

  URLVisitAggregate visit_aggregate_with_remote_tab_data(
      base::StrCat({kSampleSearchUrl, "2"}));
  visit_aggregate_with_remote_tab_data.fetcher_data_map.emplace(
      Fetcher::kSession,
      CreateSampleTabData(u"sample_title_2", URLVisit::Source::kForeign, now));

  std::vector<URLVisitAggregate> candidates = {};
  candidates.push_back(std::move(visit_aggregate_with_local_tab_data));
  candidates.push_back(std::move(visit_aggregate_with_remote_tab_data));

  FetchOptions fetch_options = FetchOptions::CreateFetchOptionsForTabResumption(
      {URLVisitAggregate::URLType::kActiveLocalTab,
       URLVisitAggregate::URLType::kActiveRemoteTab});
  fetch_options.exclude_results_containing_types = {
      URLVisitAggregate::URLType::kActiveLocalTab};
  URLVisitAggregatesTransformerTest::Result result =
      TransformAndGetResult(std::move(candidates), fetch_options);

  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), 1u);
  ASSERT_FALSE(result.second.front().GetURLTypes().Has(
      URLVisitAggregate::URLType::kActiveLocalTab));
}

}  // namespace
}  // namespace visited_url_ranking
