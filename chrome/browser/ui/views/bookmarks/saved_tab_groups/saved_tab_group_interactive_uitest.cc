// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"

class SavedTabGroupInteractiveTest : public InteractiveBrowserTest {
 public:
  SavedTabGroupInteractiveTest() = default;
  ~SavedTabGroupInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kTabGroupsSave}, {});
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTest::SetUp();
  }

  MultiStep ShowBookmarksBar() {
    return Steps(MoveMouseTo(kAppMenuButtonElementId), ClickMouse(),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }

  MultiStep HoverTabAt(int index) {
    const char kTabToHover[] = "Tab to hover";
    return Steps(NameDescendantViewByType<Tab>(kBrowserViewElementId,
                                               kTabToHover, index),
                 MoveMouseTo(kTabToHover));
  }

  MultiStep HoverFirstTabGroupHeader() {
    const char kTabGroupHeaderToHover[] = "Tab group header to hover";
    return Steps(NameDescendantViewByType<TabGroupHeader>(
                     kBrowserViewElementId, kTabGroupHeaderToHover, 0),
                 MoveMouseTo(kTabGroupHeaderToHover));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest, CreateGroupAndSave) {
  RunTestSequence(
      ShowBookmarksBar(),
      // Ensure no tab groups save buttons in the bookmarks bar are present.
      EnsureNotPresent(kSavedTabGroupButtonElementId),
      // Right click anywhere on the tab to open the context menu.
      HoverTabAt(0), ClickMouse(ui_controls::RIGHT),
      // Select option to create a new tab group and wait for the tab group
      // editor bubble to appear.
      SelectMenuItem(TabMenuModel::kAddToNewGroupItemIdentifier),
      WaitForShow(kTabGroupEditorBubbleId),
      // Click the save toggle and make sure the saved tab group appears in the
      // bookmarks bar.
      MoveMouseTo(kTabGroupEditorBubbleSaveToggleId), ClickMouse(),
      WaitForShow(kSavedTabGroupButtonElementId));
}
