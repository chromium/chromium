// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_page_action_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::ReturnRef;

class BookmarkPageActionControllerTest : public testing::Test {
 public:
  BookmarkPageActionControllerTest() {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ON_CALL(tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(tab_interface_, GetContents())
        .WillByDefault(Return(web_contents_.get()));

    pref_service_.registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled, false);

    bookmark_page_action_controller_ =
        std::make_unique<BookmarkPageActionController>(
            tab_interface_, &pref_service_, page_action_controller_);
  }

  BookmarkPageActionControllerTest(const BookmarkPageActionControllerTest&) =
      delete;
  BookmarkPageActionControllerTest& operator=(
      const BookmarkPageActionControllerTest&) = delete;

  ~BookmarkPageActionControllerTest() override = default;

  page_actions::MockPageActionController& page_action_controller() {
    return page_action_controller_;
  }

  BookmarkPageActionController& bookmark_page_action_controller() {
    return *bookmark_page_action_controller_.get();
  }

  TestingPrefServiceSimple& pref_service() { return pref_service_; }

  tabs::MockTabInterface& tab() { return tab_interface_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  tabs::MockTabInterface tab_interface_;
  TestingProfile profile_;
  TestingPrefServiceSimple pref_service_;
  page_actions::MockPageActionController page_action_controller_;
  ui::UnownedUserDataHost user_data_host_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<BookmarkPageActionController>
      bookmark_page_action_controller_;
};

TEST_F(BookmarkPageActionControllerTest, URLStarredChangedUpdatesImageAndName) {
  EXPECT_CALL(page_action_controller(),
              OverrideAccessibleName(kActionBookmarkThisTab, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionBookmarkThisTab, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideImage(kActionBookmarkThisTab, _, _))
      .Times(1);

  bookmark_page_action_controller().URLStarredChanged(tab().GetContents(),
                                                      false);

  EXPECT_CALL(page_action_controller(),
              OverrideAccessibleName(kActionBookmarkThisTab, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionBookmarkThisTab, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideImage(kActionBookmarkThisTab, _, _))
      .Times(1);

  bookmark_page_action_controller().URLStarredChanged(tab().GetContents(),
                                                      true);
}

TEST_F(BookmarkPageActionControllerTest, EditBookmarkPrefControlsVisibility) {
  EXPECT_CALL(page_action_controller(), Show(kActionBookmarkThisTab)).Times(1);
  pref_service().SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, true);

  EXPECT_CALL(page_action_controller(), Hide(kActionBookmarkThisTab)).Times(1);
  pref_service().SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
}

TEST_F(BookmarkPageActionControllerTest,
       RecordPageActionExecutionRecordsHistogram) {
  base::HistogramTester histogram_tester;

  BookmarkPageActionController::RecordPageActionExecution(
      page_actions::PageActionTrigger::kMouse);
  histogram_tester.ExpectUniqueSample("Bookmarks.EntryPoint",
                                      BookmarkEntryPoint::kStarMouse, 1);

  BookmarkPageActionController::RecordPageActionExecution(
      page_actions::PageActionTrigger::kKeyboard);
  histogram_tester.ExpectBucketCount("Bookmarks.EntryPoint",
                                     BookmarkEntryPoint::kStarKey, 1);

  BookmarkPageActionController::RecordPageActionExecution(
      page_actions::PageActionTrigger::kGesture);
  histogram_tester.ExpectBucketCount("Bookmarks.EntryPoint",
                                     BookmarkEntryPoint::kStarGesture, 1);
}
