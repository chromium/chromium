// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/test/browser_test.h"

using SidePanelBookmarksListTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelBookmarksListTest, All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/bookmarks_list_test.js", "mocha.run()");
}

using SidePanelPowerBookmarksContextMenuTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksContextMenuTest, All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/power_bookmarks_context_menu_test.js",
          "mocha.run()");
}

using SidePanelPowerBookmarksEditDialogTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksEditDialogTest, All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/power_bookmarks_edit_dialog_test.js",
          "mocha.run()");
}

using SidePanelPowerBookmarksListTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksListTest, All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/power_bookmarks_list_test.js", "mocha.run()");
}

using SidePanelPowerBookmarksServiceTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksServiceTest, All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/power_bookmarks_service_test.js",
          "mocha.run()");
}

using ShoppingListTest = WebUIMochaBrowserTest;

// TODO(crbug.com/1396268): Flaky on Mac. Re-enable this test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_All DISABLED_All
#else
#define MAYBE_All All
#endif
IN_PROC_BROWSER_TEST_F(ShoppingListTest, MAYBE_All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/commerce/shopping_list_test.js", "mocha.run()");
}

using SidePanelBookmarkFolderTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelBookmarkFolderTest, All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/bookmark_folder_test.js", "mocha.run()");
}

using SidePanelBookmarksDragManagerTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelBookmarksDragManagerTest, All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/bookmarks_drag_manager_test.js", "mocha.run()");
}

using SidePanelPowerBookmarksDragManagerTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksDragManagerTest, All) {
  set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  RunTest("side_panel/bookmarks/power_bookmarks_drag_manager_test.js",
          "mocha.run()");
}

using ReadingListAppTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(ReadingListAppTest, All) {
  set_test_loader_host(chrome::kChromeUIReadLaterHost);
  RunTest("side_panel/reading_list/reading_list_app_test.js", "mocha.run()");
}

class UserNotesBrowserTest : public WebUIMochaBrowserTest {
 protected:
  UserNotesBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {user_notes::kUserNotes, power_bookmarks::kPowerBookmarkBackend}, {});
    set_test_loader_host(chrome::kChromeUIUserNotesSidePanelHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using UserNotesAppTest = UserNotesBrowserTest;
IN_PROC_BROWSER_TEST_F(UserNotesAppTest, All) {
  RunTest("side_panel/user_notes/app_test.js", "mocha.run()");
}

using UserNoteOverviewsListTest = UserNotesBrowserTest;
IN_PROC_BROWSER_TEST_F(UserNoteOverviewsListTest, All) {
  RunTest("side_panel/user_notes/user_note_overviews_list_test.js",
          "mocha.run()");
}

using UserNotesListTest = UserNotesBrowserTest;
IN_PROC_BROWSER_TEST_F(UserNotesListTest, All) {
  RunTest("side_panel/user_notes/user_notes_list_test.js", "mocha.run()");
}

using ShoppingInsightsAppTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(ShoppingInsightsAppTest, All) {
  set_test_loader_host(commerce::kChromeUIShoppingInsightsSidePanelHost);
  RunTest("side_panel/commerce/shopping_insights_app_test.js", "mocha.run()");
}

class PriceTrackingSectionTest : public WebUIMochaBrowserTest {
 protected:
  PriceTrackingSectionTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        commerce::kPriceInsights,
        {
            {commerce::kPriceInsightsShowFeedbackParam, "true"},
        });
    set_test_loader_host(commerce::kChromeUIShoppingInsightsSidePanelHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PriceTrackingSectionTest, All) {
  RunTest("side_panel/commerce/price_tracking_section_test.js", "mocha.run()");
}
