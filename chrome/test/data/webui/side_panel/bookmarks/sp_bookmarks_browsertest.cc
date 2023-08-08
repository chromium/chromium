// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/test/browser_test.h"

class SidePanelBookmarksTest : public WebUIMochaBrowserTest {
 protected:
  SidePanelBookmarksTest() {
    set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  }
};

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksTest, List) {
  RunTest("side_panel/bookmarks/bookmarks_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksTest, DragManager) {
  RunTest("side_panel/bookmarks/bookmarks_drag_manager_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksTest, BookmarkFolder) {
  RunTest("side_panel/bookmarks/bookmark_folder_test.js", "mocha.run()");
}

// TODO(crbug.com/1396268): Flaky on Mac. Re-enable this test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShoppingList DISABLED_ShoppingList
#else
#define MAYBE_ShoppingList ShoppingList
#endif
IN_PROC_BROWSER_TEST_F(SidePanelBookmarksTest, MAYBE_ShoppingList) {
  RunTest("side_panel/bookmarks/commerce/shopping_list_test.js", "mocha.run()");
}

using SidePanelPowerBookmarksTest = SidePanelBookmarksTest;
IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, ContextMenu) {
  RunTest("side_panel/bookmarks/power_bookmarks_context_menu_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, EditDialog) {
  RunTest("side_panel/bookmarks/power_bookmarks_edit_dialog_test.js",
          "mocha.run()");
}

// TODO(crbug.com/1466691): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_List DISABLED_List
#else
#define MAYBE_List List
#endif
IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, MAYBE_List) {
  RunTest("side_panel/bookmarks/power_bookmarks_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, Service) {
  RunTest("side_panel/bookmarks/power_bookmarks_service_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, DragManager) {
  RunTest("side_panel/bookmarks/power_bookmarks_drag_manager_test.js",
          "mocha.run()");
}
