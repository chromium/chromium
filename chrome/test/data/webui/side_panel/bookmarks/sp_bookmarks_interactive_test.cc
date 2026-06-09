// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ui_features.h"
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

// TODO(crbug.com/521419448): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DragManager DISABLED_DragManager
#else
#define MAYBE_DragManager DragManager
#endif
IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, MAYBE_DragManager) {
  RunTest("side_panel/bookmarks/power_bookmarks_drag_manager_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SidePanelPowerBookmarksTest, Labels) {
  RunTest("side_panel/bookmarks/power_bookmarks_labels_test.js", "mocha.run()");
}

using SidePanelBookmarksAppTest = SidePanelBookmarksTest;
IN_PROC_BROWSER_TEST_F(SidePanelBookmarksAppTest, General1) {
  SidePanelBookmarksTest::RunTest(
      "side_panel/bookmarks/power_bookmarks_app_test.js",
      "runMochaSuite('General Part1');");
}

IN_PROC_BROWSER_TEST_F(SidePanelBookmarksAppTest, General2) {
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

// TODO(crbug.com/493823435) Investigate why this is failing.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64))
#define MAYBE_TreeView DISABLED_TreeView
#else
#define MAYBE_TreeView TreeView
#endif
IN_PROC_BROWSER_TEST_F(SidePanelBookmarksAppTest, MAYBE_TreeView) {
  RunTest("side_panel/bookmarks/power_bookmarks_app_tree_view_test.js",
          "mocha.run()");
}

class SidePanelPowerBookmarksContextMenuTest
    : public SidePanelBookmarksTest,
      public testing::WithParamInterface<bool> {
 protected:
  SidePanelPowerBookmarksContextMenuTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kMenuSimplification);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kMenuSimplification);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SidePanelPowerBookmarksContextMenuTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SidePanelPowerBookmarksContextMenuTest, ContextMenu) {
  RunTest("side_panel/bookmarks/power_bookmarks_context_menu_test.js",
          "mocha.run()");
}
