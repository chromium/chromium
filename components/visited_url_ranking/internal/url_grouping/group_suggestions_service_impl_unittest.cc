// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"

#include "base/run_loop.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "components/visited_url_ranking/internal/url_grouping/mock_suggestions_delegate.h"
#include "components/visited_url_ranking/internal/url_grouping/tab_events_visit_transformer.h"
#include "components/visited_url_ranking/public/test_support.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;

constexpr char kTestUrl[] = "https://www.example1.com/";

URLVisitAggregate CreateVisitForTab(base::TimeDelta time_since_active,
                                    int tab_id) {
  base::Time timestamp = base::Time::Now() - time_since_active;
  auto candidate = CreateSampleURLVisitAggregate(GURL(kTestUrl), 1, timestamp,
                                                 {Fetcher::kTabModel});
  auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab = std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  tab->last_active_tab.id = tab_id;
  return candidate;
}

class MockTabEventsVisitTransformer : public TabEventsVisitTransformer {
 public:
  MockTabEventsVisitTransformer() = default;
  MockTabEventsVisitTransformer(const MockTabEventsVisitTransformer&) = delete;
  MockTabEventsVisitTransformer& operator=(
      const MockTabEventsVisitTransformer&) = delete;
  ~MockTabEventsVisitTransformer() override = default;

  MOCK_METHOD3(Transform,
               void(std::vector<URLVisitAggregate> aggregates,
                    const FetchOptions& options,
                    OnTransformCallback callback));
};

class GroupSuggestionsServiceImplTest : public testing::Test {
 public:
  GroupSuggestionsServiceImplTest() = default;
  ~GroupSuggestionsServiceImplTest() override = default;

  void SetUp() override {
    Test::SetUp();
    mock_ranking_service_ = std::make_unique<MockVisitedURLRankingService>();
    mock_transformer_ = std::make_unique<MockTabEventsVisitTransformer>();
    mock_delegate_ = std::make_unique<MockGroupSuggestionsDelegate>();
    suggestions_service_ = std::make_unique<GroupSuggestionsServiceImpl>(
        mock_ranking_service_.get(), mock_transformer_.get());
  }

  void TearDown() override {
    suggestions_service_.reset();
    mock_delegate_.reset();
    mock_transformer_.reset();
    mock_ranking_service_.reset();
    Test::TearDown();
  }

  std::vector<URLVisitAggregate> GetSampleCandidates() {
    std::vector<URLVisitAggregate> candidates;
    // Add 5 tabs within 600 seconds and one over 600. The first 5 tabs should
    // be grouped.
    candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
    candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
    candidates.push_back(CreateVisitForTab(base::Seconds(300), 114));
    candidates.push_back(CreateVisitForTab(base::Seconds(500), 115));
    candidates.push_back(CreateVisitForTab(base::Seconds(500), 116));
    candidates.push_back(CreateVisitForTab(base::Seconds(800), 117));
    return candidates;
  }

  GroupSuggestionsDelegate::SuggestionResponseCallback TriggerSuggestions(
      std::vector<URLVisitAggregate> candidates) {
    base::RunLoop wait_for_compute;
    suggestions_service_->group_suggestions_manager_for_testing()
        ->set_suggestion_computed_callback_for_testing(
            wait_for_compute.QuitClosure());
    VisitedURLRankingService::GetURLVisitAggregatesCallback fetch_callback;
    ON_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _))
        .WillByDefault(MoveArg<1>(&fetch_callback));

    suggestions_service_->GetTabEventTracker()->DidAddTab(1, 0);
    GroupSuggestionsDelegate::SuggestionResponseCallback response_callback;
    ON_CALL(*mock_delegate_, ShowSuggestion(_, _))
        .WillByDefault(MoveArg<1>(&response_callback));
    std::move(fetch_callback)
        .Run(ResultStatus::kSuccess, URLVisitsMetadata{},
             std::move(candidates));
    wait_for_compute.Run();
    suggestions_service_->group_suggestions_manager_for_testing()
        ->set_suggestion_computed_callback_for_testing({});
    return response_callback;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockVisitedURLRankingService> mock_ranking_service_;
  std::unique_ptr<MockTabEventsVisitTransformer> mock_transformer_;
  std::unique_ptr<MockGroupSuggestionsDelegate> mock_delegate_;
  std::unique_ptr<GroupSuggestionsServiceImpl> suggestions_service_;
};

TEST_F(GroupSuggestionsServiceImplTest, EndToEnd) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _))
      .WillOnce(Invoke([](const GroupSuggestions& group_suggestions,
                          GroupSuggestionsDelegate::SuggestionResponseCallback
                              response_callback) {
        ASSERT_EQ(1u, group_suggestions.suggestions.size());
        const GroupSuggestion& suggestion =
            group_suggestions.suggestions.front();
        EXPECT_EQ(suggestion.suggestion_reason,
                  GroupSuggestion::SuggestionReason::kRecentlyOpened);
        EXPECT_FALSE(suggestion.suggestion_id.is_null());
        EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 114, 115, 116));
        EXPECT_FALSE(suggestion.promo_contents.empty());
        EXPECT_FALSE(suggestion.promo_header.empty());
      }));
  TriggerSuggestions(GetSampleCandidates());
}

TEST_F(GroupSuggestionsServiceImplTest, NoRepeatedSuggestions) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _));
  auto response_callback1 = TriggerSuggestions(GetSampleCandidates());
  std::move(response_callback1)
      .Run(GroupSuggestionsDelegate::UserResponseMetadata());

  // Triggering suggestions again should not show since its duplicate:

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _)).Times(0);
  TriggerSuggestions(GetSampleCandidates());

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _)).Times(0);
  TriggerSuggestions(GetSampleCandidates());

  // Remove 2 tabs to generate different suggestion, that should be shown.
  auto candidates = GetSampleCandidates();
  candidates.pop_back();
  candidates.pop_back();
  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _)).Times(1);
  TriggerSuggestions(std::move(candidates));
}

TEST_F(GroupSuggestionsServiceImplTest, GroupedTabsNotIncluded) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  auto candidates = GetSampleCandidates();

  // Set group for tab ID 115.
  auto tab_data_it = candidates[3].fetcher_data_map.find(Fetcher::kTabModel);
  auto* tab = std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
  tab->last_active_tab.tab_metadata.local_tab_group_id =
      base::Token::CreateRandom();

  EXPECT_CALL(*mock_delegate_, ShowSuggestion(_, _))
      .WillOnce(Invoke([](const GroupSuggestions& group_suggestions,
                          GroupSuggestionsDelegate::SuggestionResponseCallback
                              response_callback) {
        ASSERT_EQ(1u, group_suggestions.suggestions.size());
        const GroupSuggestion& suggestion =
            group_suggestions.suggestions.front();
        // 115 not included as its part of a group.
        EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 114, 116));
      }));

  TriggerSuggestions(std::move(candidates));
}

}  // namespace
}  // namespace visited_url_ranking
