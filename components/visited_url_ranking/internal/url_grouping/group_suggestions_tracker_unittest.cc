// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_tracker.h"

#include "base/test/task_environment.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

class GroupSuggestionsTrackerTest : public testing::Test {
 public:
  GroupSuggestionsTrackerTest() = default;
  ~GroupSuggestionsTrackerTest() override = default;

  void SetUp() override {
    Test::SetUp();
    tracker_ = std::make_unique<GroupSuggestionsTracker>();
  }

  void TearDown() override {
    tracker_.reset();
    Test::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<GroupSuggestionsTracker> tracker_;
};

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_EmptySuggestion) {
  GroupSuggestion suggestion;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion));
}

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_UnknownReason) {
  GroupSuggestion suggestion;
  suggestion.tab_ids = {1, 2, 3};
  suggestion.suggestion_reason = GroupSuggestion::SuggestionReason::kUnknown;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion));
}

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_FirstTime) {
  GroupSuggestion suggestion;
  suggestion.tab_ids = {1, 2, 3};
  suggestion.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion));
}

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_OverlappingTabs) {
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  tracker_->AddSuggestion(suggestion1,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {3, 4, 5};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion2));

  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {4, 5, 6};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion3));

  tracker_->AddSuggestion(suggestion3,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion4;
  suggestion4.tab_ids = {1, 4, 7};
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion4));
}

TEST_F(GroupSuggestionsTrackerTest, ShouldShowSuggestion_DifferentReasons) {
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  tracker_->AddSuggestion(suggestion1,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {1, 2, 3};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion2));

  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {1, 2, 3};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSimilarSource;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion3));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_OldSuggestionsRemoved) {
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  tracker_->AddSuggestion(suggestion1,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);

  task_environment_.FastForwardBy(base::Hours(25));

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {1, 2, 3};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kRecentlyOpened;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion2));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_OverlappingTabs_SwitchedBetween) {
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  tracker_->AddSuggestion(suggestion1,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {1, 2};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion2));

  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {1, 3};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion3));
  tracker_->AddSuggestion(suggestion3,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion4;
  suggestion4.tab_ids = {2, 3};
  suggestion4.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSwitchedBetween;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion4));
}

TEST_F(GroupSuggestionsTrackerTest,
       ShouldShowSuggestion_OverlappingTabs_SimilarSource) {
  GroupSuggestion suggestion1;
  suggestion1.tab_ids = {1, 2, 3};
  suggestion1.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSimilarSource;
  tracker_->AddSuggestion(suggestion1,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion2;
  suggestion2.tab_ids = {3, 4, 5};
  suggestion2.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSimilarSource;
  EXPECT_TRUE(tracker_->ShouldShowSuggestion(suggestion2));

  tracker_->AddSuggestion(suggestion2,
                          GroupSuggestionsDelegate::UserResponse::kAccepted);

  GroupSuggestion suggestion3;
  suggestion3.tab_ids = {4, 5, 6};
  suggestion3.suggestion_reason =
      GroupSuggestion::SuggestionReason::kSimilarSource;
  EXPECT_FALSE(tracker_->ShouldShowSuggestion(suggestion3));
}

}  // namespace visited_url_ranking
