// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"

#include "base/test/gmock_move_support.h"
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
  tab->last_active_tab.tab_metadata.tab_creation_time = timestamp;
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
    // Add 3 tabs within 600 seconds and one over 600. The first 3 tabs should
    // be grouped.
    candidates.push_back(CreateVisitForTab(base::Seconds(60), 111));
    candidates.push_back(CreateVisitForTab(base::Seconds(250), 112));
    candidates.push_back(CreateVisitForTab(base::Seconds(300), 114));
    candidates.push_back(CreateVisitForTab(base::Seconds(800), 116));
    return candidates;
  }

 protected:
  std::unique_ptr<MockVisitedURLRankingService> mock_ranking_service_;
  std::unique_ptr<MockTabEventsVisitTransformer> mock_transformer_;
  std::unique_ptr<MockGroupSuggestionsDelegate> mock_delegate_;
  std::unique_ptr<GroupSuggestionsServiceImpl> suggestions_service_;
};

TEST_F(GroupSuggestionsServiceImplTest, EndToEnd) {
  suggestions_service_->RegisterDelegate(mock_delegate_.get(),
                                         GroupSuggestionsService::Scope());

  VisitedURLRankingService::GetURLVisitAggregatesCallback fetch_callback;
  EXPECT_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _))
      .WillOnce(MoveArg<1>(&fetch_callback));
  suggestions_service_->GetTabEventTracker()->DidAddTab(1, 0);

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
        EXPECT_THAT(suggestion.tab_ids, ElementsAre(111, 112, 114));
        EXPECT_FALSE(suggestion.promo_contents.empty());
        EXPECT_FALSE(suggestion.promo_header.empty());
      }));

  std::move(fetch_callback)
      .Run(ResultStatus::kSuccess, URLVisitsMetadata{}, GetSampleCandidates());
}

}  // namespace
}  // namespace visited_url_ranking
