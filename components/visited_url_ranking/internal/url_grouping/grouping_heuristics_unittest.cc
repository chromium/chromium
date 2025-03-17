// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/grouping_heuristics.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

constexpr char kTestUrl[] = "https://www.example1.com/";

URLVisitAggregate CreateVisitForTab(base::TimeDelta time_since_active,
                                    int tab_id) {
  base::Time now = base::Time::Now();
  auto candidate = CreateSampleURLVisitAggregate(
      GURL(kTestUrl), 1, now - time_since_active, {Fetcher::kTabModel});
  auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
  std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second)
      ->last_active_tab.id = tab_id;
  return candidate;
}

class GroupingHeuristicsTest : public testing::Test {
 public:
  GroupingHeuristicsTest() = default;
  ~GroupingHeuristicsTest() override = default;

  void SetUp() override {
    Test::SetUp();
    heuristics_ = std::make_unique<GroupingHeuristics>();
  }

  void TearDown() override {
    heuristics_.reset();
    Test::TearDown();
  }

  std::optional<GroupSuggestions> GetSuggestionsFor(
      std::vector<URLVisitAggregate> candidates,
      GroupSuggestion::SuggestionReason reason) {
    base::test::TestFuture<std::optional<GroupSuggestions>> suggestions_future;
    heuristics_->GetSuggestions(std::move(candidates), {reason},
                                suggestions_future.GetCallback());
    return suggestions_future.Take();
  }

 protected:
  std::unique_ptr<GroupingHeuristics> heuristics_;
};

TEST_F(GroupingHeuristicsTest, RecentlyOpenedHeuristic) {
  std::vector<URLVisitAggregate> candidates = {};

  // Entries with only tabs:
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 113));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kRecentlyOpened,
            suggestion.suggestion_reason);
  EXPECT_EQ(2u, suggestion.tab_ids.size());
  EXPECT_EQ(111, suggestion.tab_ids[0]);
  EXPECT_EQ(112, suggestion.tab_ids[1]);
  EXPECT_EQ("Group recently opened tabs?", suggestion.promo_header);
  EXPECT_EQ("Organize recently opened tabs", suggestion.promo_contents);
  EXPECT_EQ(u"today", suggestion.suggested_name);
}

TEST_F(GroupingHeuristicsTest, RecentlyOpenedHeuristicNoSuggestions) {
  std::vector<URLVisitAggregate> candidates;

  // Entries with only old tabs:
  candidates.push_back(CreateVisitForTab(base::Seconds(700), 111));
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 112));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, RecentlyOpenedHeuristicSingleTab) {
  std::vector<URLVisitAggregate> candidates;

  candidates.push_back(CreateVisitForTab(base::Seconds(100), 111));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, RecentlyOpenedHeuristicEmptyAggregates) {
  std::vector<URLVisitAggregate> candidates;
  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_FALSE(suggestions.has_value());
}

}  // namespace visited_url_ranking
