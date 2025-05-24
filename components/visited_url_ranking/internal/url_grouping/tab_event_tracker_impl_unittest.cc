// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/visited_url_ranking/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

namespace {

constexpr char kTestUrl[] = "https://www.example1.com/";
}  // namespace

class TabEventTrackerImplTest : public testing::Test {
 public:
  TabEventTrackerImplTest() = default;
  ~TabEventTrackerImplTest() override = default;

  void SetUp() override {
    Test::SetUp();
    tab_event_tracker_ =
        std::make_unique<TabEventTrackerImpl>(mock_callback_.Get());
  }

  void TearDown() override {
    tab_event_tracker_.reset();
    Test::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList features_;
  base::MockCallback<TabEventTrackerImpl::OnNewEventCallback> mock_callback_;
  std::unique_ptr<TabEventTrackerImpl> tab_event_tracker_;
};

TEST_F(TabEventTrackerImplTest, CallbackCalled) {
  EXPECT_CALL(mock_callback_, Run());
  tab_event_tracker_->DidAddTab(1, 0);

  EXPECT_CALL(mock_callback_, Run());
  tab_event_tracker_->DidSelectTab(
      1, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 2);
}

TEST_F(TabEventTrackerImplTest, SwitchedCount) {
  const int kTabId1 = 1;
  const int kTabId2 = 2;

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  // Add does not change counts.
  tab_event_tracker_->DidAddTab(1, 0);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  // Move does not change counts.
  tab_event_tracker_->DidMoveTab(1, 2, 3);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  // Selection with no current tab change does not change counts.
  tab_event_tracker_->DidSelectTab(
      1, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 1);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->DidSelectTab(
      2, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 1);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->DidEnterTabSwitcher();
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->DidSelectTab(
      1, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 2);
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId2));
}

TEST_F(TabEventTrackerImplTest, SwitchedCount_IgnoreOldSwitch) {
  const int kTabId1 = 1;
  const int kTabId2 = 2;

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->DidSelectTab(
      1, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 2);
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  task_environment_.FastForwardBy(base::Minutes(6));

  tab_event_tracker_->DidSelectTab(
      2, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 1);
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId2));

  task_environment_.FastForwardBy(base::Minutes(6));

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId2));

  task_environment_.FastForwardBy(base::Minutes(6));

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));
}

TEST_F(TabEventTrackerImplTest, SwitchedCount_CloseTab) {
  const int kTabId1 = 1;
  const int kTabId2 = 2;

  tab_event_tracker_->DidSelectTab(
      2, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 1);
  tab_event_tracker_->DidSelectTab(
      1, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 2);

  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->WillCloseTab(1);

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->TabClosureCommitted(2);

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->TabClosureUndone(1);

  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));
}

TEST_F(TabEventTrackerImplTest, SwitchedCount_IgnoreNTP) {
  const int kTabId1 = 1;
  const int kTabId2 = 2;

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->DidSelectTab(
      1, GURL(TabEventTrackerImpl::kAndroidNativeNewTabPageURL),
      TabEventTracker::TabSelectionType::kFromUser, 2);
  tab_event_tracker_->DidSelectTab(
      2, GURL(TabEventTrackerImpl::kAndroidNativeNewTabPageURL),
      TabEventTracker::TabSelectionType::kFromUser, 1);

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));
}

TEST_F(TabEventTrackerImplTest, DidEnterTabSwitcher) {
  EXPECT_CALL(mock_callback_, Run()).Times(0);
  tab_event_tracker_->DidEnterTabSwitcher();

  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_tab_switcher_only", "true"}});
  tab_event_tracker_.reset();
  tab_event_tracker_ =
      std::make_unique<TabEventTrackerImpl>(mock_callback_.Get());

  EXPECT_CALL(mock_callback_, Run()).Times(1);
  tab_event_tracker_->DidEnterTabSwitcher();
}

TEST_F(TabEventTrackerImplTest, SwitchedCount_IgnoreTypes) {
  const int kTabId1 = 1;
  const int kTabId2 = 2;

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->DidSelectTab(
      1, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromCloseActiveTab,
      2);
  tab_event_tracker_->DidSelectTab(
      2, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUndoClosure,
      1);

  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId2));

  tab_event_tracker_->DidSelectTab(
      1, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 2);
  tab_event_tracker_->DidSelectTab(
      2, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromOmnibox, 1);

  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId1));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId2));
}

TEST_F(TabEventTrackerImplTest, OnDidFinishNavigation_CommitsSelection) {
  const int kTabId = 1;
  tab_event_tracker_->DidSelectTab(
      kTabId, GURL(kTestUrl),
      TabEventTracker::TabSelectionType::kFromCloseActiveTab, 2);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId));

  tab_event_tracker_->OnDidFinishNavigation(kTabId, ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId));

  tab_event_tracker_->OnDidFinishNavigation(kTabId, ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId));
}

TEST_F(TabEventTrackerImplTest, OnDidFinishNavigation_IgnoreNavigationTypes) {
  const int kTabId = 1;
  tab_event_tracker_->DidSelectTab(
      kTabId, GURL(kTestUrl),
      TabEventTracker::TabSelectionType::kFromCloseActiveTab, 2);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId));

  tab_event_tracker_->OnDidFinishNavigation(kTabId, ui::PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId));

  tab_event_tracker_->OnDidFinishNavigation(kTabId,
                                            ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId));

  tab_event_tracker_->OnDidFinishNavigation(
      kTabId, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                        ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId));
}

TEST_F(TabEventTrackerImplTest, OnDidFinishNavigation_NoRepeatCommit) {
  const int kTabId = 1;
  tab_event_tracker_->DidSelectTab(
      kTabId, GURL(kTestUrl), TabEventTracker::TabSelectionType::kFromUser, 2);
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId));

  tab_event_tracker_->OnDidFinishNavigation(kTabId, ui::PAGE_TRANSITION_LINK);
  tab_event_tracker_->OnDidFinishNavigation(kTabId, ui::PAGE_TRANSITION_LINK);
  EXPECT_EQ(1, tab_event_tracker_->GetSelectedCount(kTabId));
}

TEST_F(TabEventTrackerImplTest, OnDidFinishNavigation_TriggerCallback) {
  const int kTabId = 1;
  // Reset heuristics so that Recently Opened heuristics is enabled.
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_enable_recently_opened", "true"}});
  tab_event_tracker_.reset();
  tab_event_tracker_ =
      std::make_unique<TabEventTrackerImpl>(mock_callback_.Get());

  tab_event_tracker_->DidSelectTab(
      kTabId, GURL(kTestUrl),
      TabEventTracker::TabSelectionType::kFromCloseActiveTab, 2);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId));
  EXPECT_CALL(mock_callback_, Run());

  tab_event_tracker_->OnDidFinishNavigation(kTabId, ui::PAGE_TRANSITION_LINK);
}

TEST_F(TabEventTrackerImplTest, OnDidFinishNavigation_NotTriggerCallback) {
  const int kTabId = 1;
  // Reset heuristics so that trigger calculation on navigation is enabled.
  features_.InitAndEnableFeatureWithParameters(
      features::kGroupSuggestionService,
      {{"group_suggestion_trigger_calculation_on_page_load", "false"}});
  tab_event_tracker_.reset();
  tab_event_tracker_ =
      std::make_unique<TabEventTrackerImpl>(mock_callback_.Get());

  tab_event_tracker_->DidSelectTab(
      kTabId, GURL(kTestUrl),
      TabEventTracker::TabSelectionType::kFromCloseActiveTab, 2);
  EXPECT_EQ(0, tab_event_tracker_->GetSelectedCount(kTabId));
  EXPECT_CALL(mock_callback_, Run()).Times(0);

  tab_event_tracker_->OnDidFinishNavigation(kTabId, ui::PAGE_TRANSITION_LINK);
}
}  // namespace visited_url_ranking
