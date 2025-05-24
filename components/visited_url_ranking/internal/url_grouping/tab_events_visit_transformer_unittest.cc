// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/tab_events_visit_transformer.h"

#include <memory>

#include "components/visited_url_ranking/internal/transformer/transformer_test_support.h"
#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"
#include "components/visited_url_ranking/public/fetch_options.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

namespace {

constexpr char kTestUrl1[] = "https://www.example1.com/";
constexpr base::TimeDelta kTimeSinceLoad1 = base::Hours(20);
constexpr base::TimeDelta kTimeSinceLoad2 = base::Hours(40);

URLVisitAggregate CreateVisitForTab(base::TimeDelta time_since_active,
                                    int tab_id) {
  base::Time timestamp = base::Time::Now() - time_since_active;
  auto candidate = CreateSampleURLVisitAggregate(GURL(kTestUrl1), 1, timestamp,
                                                 {Fetcher::kTabModel});
  auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab_data =
      std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  tab_data->last_active_tab.id = tab_id;
  return candidate;
}

int GetRecentFgCount(const URLVisitAggregate& visit) {
  auto tab_data_it = visit.fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab = std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  return tab->recent_fg_count;
}

class TabEventsVisitTransformerTest : public URLVisitAggregatesTransformerTest {
 public:
  TabEventsVisitTransformerTest() = default;
  ~TabEventsVisitTransformerTest() override = default;

  void SetUp() override {
    Test::SetUp();
    auto transformer = std::make_unique<TabEventsVisitTransformer>();
    tab_transformer_impl_ = transformer.get();
    transformer_ = std::move(transformer);
    tab_tracker_ = std::make_unique<TabEventTrackerImpl>(base::DoNothing());
    tab_transformer_impl_->set_tab_event_tracker(tab_tracker_.get());
  }

  void TearDown() override {
    tab_transformer_impl_->set_tab_event_tracker(nullptr);
    tab_tracker_.reset();
    tab_transformer_impl_ = nullptr;
    transformer_.reset();
    Test::TearDown();
  }

  std::vector<URLVisitAggregate> GetSampleCandidates() {
    std::vector<URLVisitAggregate> candidates = {};
    candidates.push_back(CreateVisitForTab(kTimeSinceLoad1, 1));
    candidates.push_back(CreateVisitForTab(kTimeSinceLoad2, 2));

    tab_tracker_->DidAddTab(1, 1);
    tab_tracker_->DidAddTab(2, 1);
    tab_tracker_->DidSelectTab(1, GURL(kTestUrl1),
                               TabEventTracker::TabSelectionType::kFromUser, 2);
    tab_tracker_->DidSelectTab(2, GURL(kTestUrl1),
                               TabEventTracker::TabSelectionType::kFromUser, 1);
    tab_tracker_->DidSelectTab(1, GURL(kTestUrl1),
                               TabEventTracker::TabSelectionType::kFromUser, 2);

    return candidates;
  }

 protected:
  raw_ptr<TabEventsVisitTransformer> tab_transformer_impl_;
  std::unique_ptr<TabEventTrackerImpl> tab_tracker_;
};

TEST_F(TabEventsVisitTransformerTest, Transform) {
  auto candidates = GetSampleCandidates();
  URLVisitAggregatesTransformerTest::Result result = TransformAndGetResult(
      std::move(candidates),
      FetchOptions::CreateDefaultFetchOptionsForTabResumption());
  ASSERT_EQ(result.first, URLVisitAggregatesTransformer::Status::kSuccess);
  ASSERT_EQ(result.second.size(), 2u);
  EXPECT_EQ(GetRecentFgCount(result.second[0]), 2);
  EXPECT_EQ(GetRecentFgCount(result.second[1]), 1);
}

}  // namespace

}  // namespace visited_url_ranking
