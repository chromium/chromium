// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class SidePanelBookmarksTest : public WebUIMochaFocusTest {
 protected:
  SidePanelBookmarksTest() {
    set_test_loader_host(chrome::kChromeUIBookmarksSidePanelHost);
  }
};

// TODO(crbug.com/40882667): Flaky on Mac, Linux dbg, and Windows. Re-enable
// this test.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) && !defined(NDEBUG)) || \
    BUILDFLAG(IS_WIN)
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

using SidePanelBookmarksAppTest = SidePanelBookmarksTest;
// TODO(crbug.com/489813344): Flaky test.
IN_PROC_BROWSER_TEST_F(SidePanelBookmarksAppTest, DISABLED_General1) {
  SidePanelBookmarksTest::RunTest(
      "side_panel/bookmarks/power_bookmarks_app_test.js",
      "runMochaSuite('General Part1');");
}

// TODO(https://crbug.com/512674938): Reenable.
#if BUILDFLAG(IS_MAC)
#define MAYBE_General2 DISABLED_General2
#else
#define MAYBE_General2 General2
#endif
IN_PROC_BROWSER_TEST_F(SidePanelBookmarksAppTest, MAYBE_General2) {
  SidePanelBookmarksTest::RunTest(
      "side_panel/bookmarks/power_bookmarks_app_test.js",
      "runMochaSuite('General Part2');");
}

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksAppTest, BookmarksMigrateUiChanges) {
  RunTest(
      "side_panel/bookmarks/"
      "power_bookmarks_app_migrate_ui_changes_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksAppTest, TreeView) {
  RunTest("side_panel/bookmarks/power_bookmarks_app_tree_view_test.js",
          "mocha.run()");
}
