// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "content/public/test/browser_test.h"

class SidePanelBookmarksTest : public WebUIMochaFocusTest {
 protected:
  SidePanelBookmarksTest() {
    set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  }
};

// TODO(crbug.com/40882667): Flaky on Mac and Linux dbg. Re-enable this test.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) && !defined(NDEBUG))
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

IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, Service) {
  RunTest("side_panel/bookmarks/power_bookmarks_service_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest,
                       KeyboardArrowNavigationService) {
  RunTest("side_panel/bookmarks/keyboard_arrow_navigation_service_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, DragManager) {
  RunTest("side_panel/bookmarks/power_bookmarks_drag_manager_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, Labels) {
  RunTest("side_panel/bookmarks/power_bookmarks_labels_test.js", "mocha.run()");
}

using SidePanelBookmarksListTest = SidePanelBookmarksTest;
IN_PROC_BROWSER_TEST_F(SidePanelBookmarksListTest, General1) {
  SidePanelBookmarksTest::RunTest(
      "side_panel/bookmarks/power_bookmarks_list_test.js",
      "runMochaSuite('General Part1');");
}

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksListTest, General2) {
  SidePanelBookmarksTest::RunTest(
      "side_panel/bookmarks/power_bookmarks_list_test.js",
      "runMochaSuite('General Part2');");
}

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksListTest, TransportMode) {
  RunTest("side_panel/bookmarks/power_bookmarks_list_transport_mode_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksListTest, TreeView) {
  RunTest("side_panel/bookmarks/power_bookmarks_list_tree_view_test.js",
          "mocha.run()");
}
