// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_manager.h"

#include "components/sessions/core/session_id.h"
#include "components/visited_url_ranking/internal/url_grouping/mock_suggestions_delegate.h"
#include "components/visited_url_ranking/public/testing/mock_visited_url_ranking_service.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace visited_url_ranking {

class GroupSuggestionsManagerTest : public testing::Test {
 public:
  GroupSuggestionsManagerTest() = default;
  ~GroupSuggestionsManagerTest() override = default;

  void SetUp() override {
    Test::SetUp();
    mock_ranking_service_ = std::make_unique<MockVisitedURLRankingService>();
    suggestions_manager_ =
        std::make_unique<GroupSuggestionsManager>(mock_ranking_service_.get());
  }

  void TearDown() override {
    suggestions_manager_.reset();
    Test::TearDown();
  }

 protected:
  std::unique_ptr<MockVisitedURLRankingService> mock_ranking_service_;
  std::unique_ptr<GroupSuggestionsManager> suggestions_manager_;
};

TEST_F(GroupSuggestionsManagerTest, RegisterDelegate) {
  MockGroupSuggestionsDelegate delegate;
  GroupSuggestionsService::Scope scope{.tab_session_id =
                                           SessionID::NewUnique()};
  suggestions_manager_->UnregisterDelegate(nullptr);
  suggestions_manager_->UnregisterDelegate(&delegate);

  suggestions_manager_->RegisterDelegate(&delegate, scope);
  suggestions_manager_->RegisterDelegate(&delegate, scope);

  suggestions_manager_->UnregisterDelegate(&delegate);
  suggestions_manager_->UnregisterDelegate(&delegate);
}

TEST_F(GroupSuggestionsManagerTest, TriggerSuggestions) {
  GroupSuggestionsService::Scope scope{.tab_session_id =
                                           SessionID::NewUnique()};
  GroupSuggestionsService::Scope scope1{.tab_session_id =
                                            SessionID::NewUnique()};

  EXPECT_FALSE(suggestions_manager_->GetCurrentComputationForTesting());

  EXPECT_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _));
  suggestions_manager_->MaybeTriggerSuggestions(scope);
  EXPECT_TRUE(suggestions_manager_->GetCurrentComputationForTesting());

  EXPECT_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _));
  suggestions_manager_->MaybeTriggerSuggestions(scope);
  EXPECT_TRUE(suggestions_manager_->GetCurrentComputationForTesting());

  EXPECT_CALL(*mock_ranking_service_, FetchURLVisitAggregates(_, _));
  suggestions_manager_->MaybeTriggerSuggestions(scope1);
  EXPECT_TRUE(suggestions_manager_->GetCurrentComputationForTesting());
}

}  // namespace visited_url_ranking
