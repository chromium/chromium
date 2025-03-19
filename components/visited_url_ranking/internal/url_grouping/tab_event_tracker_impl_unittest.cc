// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"

#include "base/test/mock_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace visited_url_ranking {

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
  base::MockCallback<TabEventTrackerImpl::OnNewEventCallback> mock_callback_;
  std::unique_ptr<TabEventTrackerImpl> tab_event_tracker_;
};

TEST_F(TabEventTrackerImplTest, CallbackCalled) {
  EXPECT_CALL(mock_callback_, Run());
  tab_event_tracker_->DidAddTab(1, 0);

  EXPECT_CALL(mock_callback_, Run());
  tab_event_tracker_->DidMoveTab(1, 2, 3);

  EXPECT_CALL(mock_callback_, Run());
  tab_event_tracker_->DidSelectTab(
      1, TabEventTracker::TabSelectionType::kFromAppExit, 2);

  EXPECT_CALL(mock_callback_, Run());
  tab_event_tracker_->DidEnterTabSwitcher();
}

}  // namespace visited_url_ranking
