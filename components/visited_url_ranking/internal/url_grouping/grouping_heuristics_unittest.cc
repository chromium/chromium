// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/grouping_heuristics.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/tab_metadata.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

using ::testing::ElementsAre;

constexpr char kTestUrl[] = "https://www.example1.com/";

URLVisitAggregate CreateVisitForTab(base::TimeDelta time_since_active,
                                    int tab_id) {
  base::Time timestamp = base::Time::Now() - time_since_active;
  auto candidate = CreateSampleURLVisitAggregate(GURL(kTestUrl), 1, timestamp,
                                                 {Fetcher::kTabModel});
  auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab_data =
      std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  tab_data->last_active_tab.id = tab_id;
  tab_data->last_active_tab.tab_metadata.tab_origin =
      TabMetadata::TabOrigin::kOpenedByUserAction;
  tab_data->last_active_tab.tab_metadata.tab_creation_time = timestamp;
  return candidate;
}

void SetRecentFgCount(URLVisitAggregate& visit, int count) {
  auto tab_data_it = visit.fetcher_data_map.find(Fetcher::kTabModel);
  std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second)
      ->recent_fg_count = count;
}

TabMetadata& GetTabMetadata(URLVisitAggregate& visit) {
  auto tab_data_it = visit.fetcher_data_map.find(Fetcher::kTabModel);
  return std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second)
      ->last_active_tab.tab_metadata;
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

  std::optional<GroupSuggestions> GetSuggestionsFor(
      std::vector<URLVisitAggregate> candidates,
      std::vector<GroupSuggestion::SuggestionReason> reason) {
    base::test::TestFuture<std::optional<GroupSuggestions>> suggestions_future;
    heuristics_->GetSuggestions(std::move(candidates), reason,
                                suggestions_future.GetCallback());
    return suggestions_future.Take();
  }

 protected:
  base::test::ScopedFeatureList features_;
  std::unique_ptr<GroupingHeuristics> heuristics_;
};

TEST_F(GroupingHeuristicsTest, HeuristicsSingleTab) {
  std::vector<URLVisitAggregate> candidates;

  candidates.push_back(CreateVisitForTab(base::Seconds(100), 111));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        {
                            GroupSuggestion::SuggestionReason::kRecentlyOpened,
                            GroupSuggestion::SuggestionReason::kSwitchedBetween,
                            GroupSuggestion::SuggestionReason::kSimilarSource,
                        });
  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, HeuristicsEmptyAggregates) {
  std::vector<URLVisitAggregate> candidates;

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        {
                            GroupSuggestion::SuggestionReason::kRecentlyOpened,
                            GroupSuggestion::SuggestionReason::kSwitchedBetween,
                            GroupSuggestion::SuggestionReason::kSimilarSource,
                        });
  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, RecentlyOpenedHeuristic) {
  std::vector<URLVisitAggregate> candidates = {};

  // 4 tabs are below 600 seconds time limit to be considered recent and should
  // be grouped.
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 114));
  candidates.push_back(CreateVisitForTab(base::Seconds(30), 115));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kRecentlyOpened,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 113, 115));
  EXPECT_EQ("Group recently opened tabs?", suggestion.promo_header);
  EXPECT_EQ("Organize recently opened tabs.", suggestion.promo_contents);
  EXPECT_EQ(u"today", suggestion.suggested_name);
}

TEST_F(GroupingHeuristicsTest, RecentlyOpenedHeuristicNoSuggestions) {
  std::vector<URLVisitAggregate> candidates;

  // All 3 tabs are over the time limit.
  candidates.push_back(CreateVisitForTab(base::Seconds(700), 111));
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 112));
  candidates.push_back(CreateVisitForTab(base::Seconds(1000), 113));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, SwitchedBetweenHeuristic) {
  std::vector<URLVisitAggregate> candidates = {};

  // First 2 tabs have more than 2 foreground switches.
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  SetRecentFgCount(candidates[0], 2);
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  SetRecentFgCount(candidates[1], 3);
  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  SetRecentFgCount(candidates[2], 0);
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 114));
  SetRecentFgCount(candidates[3], 1);

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kSwitchedBetween);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kSwitchedBetween,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112));
  EXPECT_EQ("Group tabs in bottom tab strip?", suggestion.promo_header);
  EXPECT_EQ("Switch between tabs easily with tab strip at the bottom.",
            suggestion.promo_contents);
  EXPECT_EQ(u"today", suggestion.suggested_name);
}

TEST_F(GroupingHeuristicsTest, SwitchedBetweenHeuristicNoSuggestions) {
  std::vector<URLVisitAggregate> candidates;

  candidates.push_back(CreateVisitForTab(base::Seconds(700), 111));
  SetRecentFgCount(candidates[0], 1);
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 112));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kSwitchedBetween);
  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, SimilarSourceHeuristic_Package) {
  std::vector<URLVisitAggregate> candidates = {};

  // 3 tabs have the same package name.
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).launch_package_name = "package1";
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).launch_package_name = "package2";
  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  GetTabMetadata(candidates[2]).launch_package_name = "package1";
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 114));
  GetTabMetadata(candidates[3]).launch_package_name = "package1";

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kSimilarSource,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 113, 114));
  EXPECT_EQ("Group recently opened tabs?", suggestion.promo_header);
  EXPECT_EQ("Organize recent tabs opened using the same action.",
            suggestion.promo_contents);
  EXPECT_EQ(u"today", suggestion.suggested_name);
}

TEST_F(GroupingHeuristicsTest, SimilarSourceHeuristic_AutoOpenNotIncluded) {
  std::vector<URLVisitAggregate> candidates = {};

  // All tabs have the same package name, but one is not opened by user.

  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).launch_package_name = "package1";

  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).launch_package_name = "package1";
  GetTabMetadata(candidates[1]).tab_origin =
      TabMetadata::TabOrigin::kOpenedWithoutUserAction;

  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  GetTabMetadata(candidates[2]).launch_package_name = "package1";

  candidates.push_back(CreateVisitForTab(base::Seconds(800), 114));
  GetTabMetadata(candidates[3]).launch_package_name = "package1";

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kSimilarSource,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 113, 114));
}

TEST_F(GroupingHeuristicsTest,
       SimilarSourceHeuristic_CurrentTabBlocksSuggestion) {
  std::vector<URLVisitAggregate> candidates = {};

  // 3 tabs have same package name, but the current tab does not.
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).tab_android_launch_type = 4;
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).launch_package_name = "package1";
  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  GetTabMetadata(candidates[2]).launch_package_name = "package1";
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 114));
  GetTabMetadata(candidates[3]).launch_package_name = "package1";

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, SimilarSourceHeuristic_SameParentTabID) {
  std::vector<URLVisitAggregate> candidates = {};

  // 5 tabs but one of them is from different package, and one has different
  // paretn tab ID, so 3 are grouped:
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  // Not used since package is set.
  GetTabMetadata(candidates[1]).launch_package_name = "package1";
  GetTabMetadata(candidates[1]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  GetTabMetadata(candidates[2]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(800), 114));
  // Not used since parent id is different.
  GetTabMetadata(candidates[3]).parent_tab_id = 456;

  // Not clustered since parent ID is different.
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 115));
  GetTabMetadata(candidates[4]).parent_tab_id = 123;

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kSimilarSource,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 113, 115));
}

TEST_F(GroupingHeuristicsTest,
       SimilarSourceHeuristic_LaunchType_InvalidParentID) {
  std::vector<URLVisitAggregate> candidates = {};

  // 3 tabs have the same launch type and the same parent ID, however
  // their parent tab ID is -1 which indicates that there is no parent
  // tab, so no clustering.

  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).tab_android_launch_type = 4;
  GetTabMetadata(candidates[0]).parent_tab_id = -1;

  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).tab_android_launch_type = 4;
  GetTabMetadata(candidates[1]).parent_tab_id = -1;

  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  GetTabMetadata(candidates[2]).tab_android_launch_type = 4;
  GetTabMetadata(candidates[2]).parent_tab_id = -1;

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, DisableRecentlyOpen) {
  // Reset heuristics so that Recently Open heuristics is not enabled.
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_recently_opened", "false"}});
  heuristics_.reset();
  heuristics_ = std::make_unique<GroupingHeuristics>();

  std::vector<URLVisitAggregate> candidates = {};

  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  candidates.push_back(CreateVisitForTab(base::Seconds(30), 114));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, DisableSwitchBetween) {
  // Reset heuristics so that Switch Between heuristics is not enabled.
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_switch_between", "false"}});
  heuristics_.reset();
  heuristics_ = std::make_unique<GroupingHeuristics>();

  std::vector<URLVisitAggregate> candidates = {};

  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  SetRecentFgCount(candidates[0], 2);
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  SetRecentFgCount(candidates[1], 3);

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kSwitchedBetween);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, DisableSimilarSource) {
  // Reset heuristics so that Similar Source heuristics is not enabled.
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_similar_source", "false"}});
  heuristics_.reset();
  heuristics_ = std::make_unique<GroupingHeuristics>();

  std::vector<URLVisitAggregate> candidates = {};

  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).launch_package_name = "package1";
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).launch_package_name = "package1";
  candidates.push_back(CreateVisitForTab(base::Seconds(200), 113));
  GetTabMetadata(candidates[2]).launch_package_name = "package1";
  candidates.push_back(CreateVisitForTab(base::Seconds(200), 114));

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_FALSE(suggestions.has_value());
}

}  // namespace visited_url_ranking
