// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/grouping_heuristics.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/visited_url_ranking/public/tab_metadata.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr char kTestUrl[] = "https://www.example1.com/";
constexpr char kFooUrl1[] = "https://www.foo.com/1";
constexpr char kFooUrl2[] = "https://www.foo.com/2";
constexpr char kFooUrl3[] = "https://www.foo.com/3";
constexpr char kFooUrl4[] = "https://www.foo4.com/4";
constexpr char kFooUrl5[] = "https://www.foo5.com/5";

// Visibility threshold from grouping_heuristics.cc
const float kVisibilityThreshold = 0.7f;

// Helper to add history data with a specific visibility score.
void SetVisibilityScore(URLVisitAggregate& visit_aggregate,
                        std::optional<float> visibility_score) {
  visit_aggregate.fetcher_data_map.erase(Fetcher::kHistory);
  if (!visibility_score) {
    return;
  }
  history::AnnotatedVisit annotated_visit = GenerateSampleAnnotatedVisit(
      /*visit_id=*/1, /*page_title=*/u"Test Title", GURL(),
      /*has_url_keyed_image=*/true, /*originator_cache_guid=*/"",
      *visibility_score, /*categories=*/{}, base::Time());
  visit_aggregate.fetcher_data_map.emplace(
      Fetcher::kHistory,
      URLVisitAggregate::HistoryData(std::move(annotated_visit)));
}

URLVisitAggregate CreateVisitForTab(base::TimeDelta time_since_active,
                                    int tab_id,
                                    GURL url = GURL(kTestUrl),
                                    float visibility_score = 0.95) {
  base::Time timestamp = base::Time::Now() - time_since_active;
  auto candidate =
      CreateSampleURLVisitAggregate(url, 1, timestamp, {Fetcher::kTabModel});
  auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab_data =
      std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  tab_data->last_active_tab.id = tab_id;
  tab_data->last_active_tab.tab_metadata.tab_origin =
      TabMetadata::TabOrigin::kOpenedByUserAction;
  tab_data->last_active_tab.tab_metadata.tab_creation_time = timestamp;

  SetVisibilityScore(candidate, visibility_score);
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
    base::test::TestFuture<GroupingHeuristics::SuggestionsResult>
        suggestions_future;
    heuristics_->GetSuggestions(std::move(candidates), {reason},
                                suggestions_future.GetCallback());
    return suggestions_future.Take().suggestions;
  }

  std::optional<GroupSuggestions> GetSuggestionsFor(
      std::vector<URLVisitAggregate> candidates,
      std::vector<GroupSuggestion::SuggestionReason> reason) {
    base::test::TestFuture<GroupingHeuristics::SuggestionsResult>
        suggestions_future;
    heuristics_->GetSuggestions(std::move(candidates), reason,
                                suggestions_future.GetCallback());
    return suggestions_future.Take().suggestions;
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
  // Reset heuristics so that Recently Opened heuristics is enabled.
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_recently_opened", "true"}});
  heuristics_.reset();
  heuristics_ = std::make_unique<GroupingHeuristics>();

  std::vector<URLVisitAggregate> candidates = {};

  // 4 tabs are below 600 seconds time limit to be considered recent and should
  // be grouped.
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).is_currently_active = true;
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
  GetTabMetadata(candidates[0]).is_currently_active = true;
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
  GetTabMetadata(candidates[0]).is_currently_active = true;
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
  EXPECT_EQ("Group recently selected tabs?", suggestion.promo_header);
  EXPECT_EQ("Switch between tabs easily with tab strip at the bottom.",
            suggestion.promo_contents);
  EXPECT_EQ(u"today", suggestion.suggested_name);
}

TEST_F(GroupingHeuristicsTest, SwitchedBetweenHeuristicNoSuggestions) {
  std::vector<URLVisitAggregate> candidates;

  candidates.push_back(CreateVisitForTab(base::Seconds(700), 111));
  SetRecentFgCount(candidates[0], 1);
  GetTabMetadata(candidates[0]).is_currently_active = true;
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 112));

  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kSwitchedBetween);
  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, SimilarSourceHeuristic_AutoOpenNotIncluded) {
  std::vector<URLVisitAggregate> candidates = {};

  // All tabs have the same parent tab ID, but one is not opened by user.

  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).is_currently_active = true;
  GetTabMetadata(candidates[0]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).parent_tab_id = 123;
  GetTabMetadata(candidates[1]).tab_origin =
      TabMetadata::TabOrigin::kOpenedWithoutUserAction;

  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  GetTabMetadata(candidates[2]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(500), 114));
  GetTabMetadata(candidates[3]).parent_tab_id = 123;

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

  // 3 tabs have same parent tab ID, but the current tab does not.
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).parent_tab_id = 456;
  GetTabMetadata(candidates[0]).is_currently_active = true;
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).parent_tab_id = 123;
  GetTabMetadata(candidates[0]).is_currently_active = false;
  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  GetTabMetadata(candidates[2]).parent_tab_id = 123;
  GetTabMetadata(candidates[0]).is_currently_active = false;
  candidates.push_back(CreateVisitForTab(base::Seconds(500), 114));
  GetTabMetadata(candidates[3]).parent_tab_id = 123;
  GetTabMetadata(candidates[0]).is_currently_active = false;

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, SimilarSourceHeuristic_SameParentTabID) {
  std::vector<URLVisitAggregate> candidates = {};

  // 4 tabs but one has different parent tab ID, so 3 are grouped:
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).is_currently_active = true;
  GetTabMetadata(candidates[0]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(350), 112));
  GetTabMetadata(candidates[1]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(500), 113));
  // Not used since parent id is different.
  GetTabMetadata(candidates[2]).parent_tab_id = 456;

  candidates.push_back(CreateVisitForTab(base::Seconds(500), 114));
  GetTabMetadata(candidates[3]).parent_tab_id = 123;

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kSimilarSource,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 114));
}

TEST_F(GroupingHeuristicsTest, SimilarSourceHeuristic_RecentTabs) {
  std::vector<URLVisitAggregate> candidates = {};

  // 4 tabs with the same parent tab ID but one is not recent, so 3 are grouped:
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).is_currently_active = true;
  GetTabMetadata(candidates[0]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(350), 112));
  GetTabMetadata(candidates[1]).parent_tab_id = 123;

  // Not used since tab is not recent.
  candidates.push_back(CreateVisitForTab(base::Seconds(800), 113));
  GetTabMetadata(candidates[2]).parent_tab_id = 123;

  candidates.push_back(CreateVisitForTab(base::Seconds(500), 114));
  GetTabMetadata(candidates[3]).parent_tab_id = 123;

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kSimilarSource,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 114));
}

TEST_F(GroupingHeuristicsTest, SameOrigin) {
  // Reset heuristics so that Same Origin heuristics is enabled.
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_same_origin", "true"}});
  heuristics_.reset();
  heuristics_ = std::make_unique<GroupingHeuristics>();

  std::vector<URLVisitAggregate> candidates = {};

  // 4 tabs with 3 of them from the same origin.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(60), 111, GURL(kFooUrl1)));
  GetTabMetadata(candidates[0]).is_currently_active = true;
  candidates.push_back(
      CreateVisitForTab(base::Seconds(250), 112, GURL(kFooUrl2)));
  candidates.push_back(
      CreateVisitForTab(base::Seconds(200), 113, GURL(kFooUrl3)));
  candidates.push_back(
      CreateVisitForTab(base::Seconds(200), 114, GURL(kTestUrl)));

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSameOrigin);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kSameOrigin,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 113));
}

TEST_F(GroupingHeuristicsTest, DisableRecentlyOpen) {
  // Recently Open heuristics is disabled by default.
  std::vector<URLVisitAggregate> candidates = {};

  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).is_currently_active = true;
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
  GetTabMetadata(candidates[0]).is_currently_active = true;
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
  GetTabMetadata(candidates[0]).parent_tab_id = 123;
  GetTabMetadata(candidates[0]).is_currently_active = true;
  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).parent_tab_id = 123;
  candidates.push_back(CreateVisitForTab(base::Seconds(200), 113));
  GetTabMetadata(candidates[2]).parent_tab_id = 123;
  candidates.push_back(CreateVisitForTab(base::Seconds(200), 114));
  GetTabMetadata(candidates[3]).parent_tab_id = 123;

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, DisableSameOrigin) {
  // Same Origin heuristics is disabled by default.
  std::vector<URLVisitAggregate> candidates = {};

  candidates.push_back(
      CreateVisitForTab(base::Seconds(60), 111, GURL(kFooUrl1)));
  GetTabMetadata(candidates[0]).is_currently_active = true;
  candidates.push_back(
      CreateVisitForTab(base::Seconds(250), 112, GURL(kFooUrl2)));
  candidates.push_back(
      CreateVisitForTab(base::Seconds(200), 113, GURL(kFooUrl3)));

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSameOrigin);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest, SimilarSourceHeuristic_SameParentTabCluster) {
  std::vector<URLVisitAggregate> candidates = {};

  // The parent tab relationship for the below 6 tabs are
  // 111->112
  // (112, 113) -> 114
  // 115 -> 116
  // (111, 112, 113, 114) will be clustered.
  candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
  GetTabMetadata(candidates[0]).parent_tab_id = 112;
  GetTabMetadata(candidates[0]).is_currently_active = true;

  candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
  GetTabMetadata(candidates[1]).parent_tab_id = 114;

  candidates.push_back(CreateVisitForTab(base::Seconds(350), 113));
  GetTabMetadata(candidates[2]).parent_tab_id = 114;

  candidates.push_back(CreateVisitForTab(base::Seconds(500), 114));
  GetTabMetadata(candidates[3]).parent_tab_id = 114;

  // Not clustered since parent ID is different.
  candidates.push_back(CreateVisitForTab(base::Seconds(350), 115));
  GetTabMetadata(candidates[4]).parent_tab_id = 116;

  candidates.push_back(CreateVisitForTab(base::Seconds(500), 116));
  GetTabMetadata(candidates[4]).parent_tab_id = 116;

  std::optional<GroupSuggestions> suggestions = GetSuggestionsFor(
      std::move(candidates), GroupSuggestion::SuggestionReason::kSimilarSource);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  const auto& suggestion = suggestions->suggestions[0];
  EXPECT_EQ(GroupSuggestion::SuggestionReason::kSimilarSource,
            suggestion.suggestion_reason);
  EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 113, 114));
}

TEST_F(GroupingHeuristicsTest,
       VisibilityScore_GroupNotShown_OneTabInGroupHasInvisibleHistory) {
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_recently_opened", "true"}});

  heuristics_ =
      std::make_unique<GroupingHeuristics>();  // Re-init after features

  std::vector<URLVisitAggregate> candidates;

  // Tab 1 (Active): Recent, visible history.
  candidates.push_back(CreateVisitForTab(base::Seconds(50), 1, GURL(kFooUrl1)));
  GetTabMetadata(candidates.back()).is_currently_active = true;
  SetVisibilityScore(candidates.back(), kVisibilityThreshold + 0.1f);
  // Tab 2: Recent, INVISIBLE history.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(100), 2, GURL(kFooUrl2)));
  SetVisibilityScore(candidates.back(), kVisibilityThreshold - 0.1f);
  // Tab 3: Recent, visible history.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(150), 3, GURL(kFooUrl3)));
  SetVisibilityScore(candidates.back(), kVisibilityThreshold + 0.1f);
  // Tab 4: Recent, no history data.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(200), 4, GURL(kFooUrl4)));
  SetVisibilityScore(candidates.back(), std::nullopt);
  // Tab 5: Not recent enough, visible history.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(700), 5, GURL(kFooUrl5)));
  SetVisibilityScore(candidates.back(), kVisibilityThreshold + 0.1f);

  // Potential group: {1,2,3,4}. Tab 2 makes it not visible.
  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest,
       VisibilityScore_GroupNotShown_AllTabsInGroupLackHistory) {
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_recently_opened", "true"}});

  heuristics_ =
      std::make_unique<GroupingHeuristics>();  // Re-init after features

  std::vector<URLVisitAggregate> candidates;
  // All tabs are recent but lack history data.
  candidates.push_back(CreateVisitForTab(base::Seconds(50), 1, GURL(kFooUrl1)));
  GetTabMetadata(candidates.back()).is_currently_active = true;
  SetVisibilityScore(candidates.back(), std::nullopt);
  candidates.push_back(
      CreateVisitForTab(base::Seconds(100), 2, GURL(kFooUrl2)));
  SetVisibilityScore(candidates.back(), std::nullopt);
  candidates.push_back(
      CreateVisitForTab(base::Seconds(150), 3, GURL(kFooUrl3)));
  SetVisibilityScore(candidates.back(), std::nullopt);
  candidates.push_back(
      CreateVisitForTab(base::Seconds(200), 4, GURL(kFooUrl4)));
  SetVisibilityScore(candidates.back(), std::nullopt);
  // Tab 5: Not recent enough, also no history.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(700), 5, GURL(kFooUrl5)));
  SetVisibilityScore(candidates.back(), std::nullopt);

  // Potential group: {1,2,3,4}.
  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_FALSE(suggestions.has_value());
}

TEST_F(GroupingHeuristicsTest,
       VisibilityScore_GroupShown_MixedHistoryInGroup_VisibleAndNoHistory) {
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_recently_opened", "true"}});

  heuristics_ =
      std::make_unique<GroupingHeuristics>();  // Re-init after features

  std::vector<URLVisitAggregate> candidates;

  // Tab 1 (Active): Recent, No history data.
  candidates.push_back(CreateVisitForTab(base::Seconds(50), 1, GURL(kFooUrl1)));
  GetTabMetadata(candidates.back()).is_currently_active = true;
  // Tab 2: Recent, Visible history.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(100), 2, GURL(kFooUrl2)));
  SetVisibilityScore(candidates.back(), kVisibilityThreshold + 0.1f);
  // Tab 3: Recent, Visible history.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(150), 3, GURL(kFooUrl3)));
  SetVisibilityScore(candidates.back(), kVisibilityThreshold + 0.2f);
  // Tab 4: Recent, No history data.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(200), 4, GURL(kFooUrl4)));
  SetVisibilityScore(candidates.back(), std::nullopt);
  // Tab 5: Not recent enough, visible history.
  candidates.push_back(
      CreateVisitForTab(base::Seconds(700), 5, GURL(kFooUrl5)));
  SetVisibilityScore(candidates.back(), kVisibilityThreshold + 0.1f);

  // Potential group: {1,2,3,4}. Tabs 1 & 4 lack history (implicitly
  // visible). Tabs 2 & 3 have visible history. Intended: `IsGroupVisible`
  // returns true.
  std::optional<GroupSuggestions> suggestions =
      GetSuggestionsFor(std::move(candidates),
                        GroupSuggestion::SuggestionReason::kRecentlyOpened);

  ASSERT_TRUE(suggestions.has_value());
  ASSERT_EQ(1u, suggestions->suggestions.size());
  EXPECT_THAT(suggestions->suggestions[0].tab_ids, ElementsAre(1, 2, 3, 4));
}

}  // namespace visited_url_ranking
