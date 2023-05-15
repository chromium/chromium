// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/prefs/pref_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

class SavedTabGroupInteractiveTest : public InteractiveBrowserTest {
 public:
  SavedTabGroupInteractiveTest() = default;
  ~SavedTabGroupInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kTabGroupsSave}, {});
    InteractiveBrowserTest::SetUp();
  }

  MultiStep ShowBookmarksBar() {
    return Steps(PressButton(kAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }

  StepBuilder FinishTabstripAnimations() {
    return std::move(WithView(kTabStripElementId, [](TabStrip* tab_strip) {
                       tab_strip->StopAnimating(true);
                     }).SetDescription("FinishTabstripAnimation"));
  }

  MultiStep HoverTabAt(int index) {
    const char kTabToHover[] = "Tab to hover";
    return Steps(NameDescendantViewByType<Tab>(kBrowserViewElementId,
                                               kTabToHover, index),
                 MoveMouseTo(kTabToHover));
  }

  MultiStep HoverTabGroupHeader(tab_groups::TabGroupId group_id) {
    const char kTabGroupHeaderToHover[] = "Tab group header to hover";
    return Steps(
        FinishTabstripAnimations(),
        NameDescendantView(
            kBrowserViewElementId, kTabGroupHeaderToHover,
            base::BindRepeating(
                [](tab_groups::TabGroupId group_id, const views::View* view) {
                  const TabGroupHeader* header =
                      views::AsViewClass<TabGroupHeader>(view);
                  if (!header) {
                    return false;
                  }
                  return header->group().value() == group_id;
                },
                group_id)),
        MoveMouseTo(kTabGroupHeaderToHover));
  }

  MultiStep SaveGroupLeaveEditorBubbleOpen(tab_groups::TabGroupId group_id) {
    return Steps(EnsureNotPresent(kTabGroupEditorBubbleId),
                 // Right click on the header to open the editor bubble.
                 HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
                 // Wait for the tab group editor bubble to appear.
                 WaitForShow(kTabGroupEditorBubbleId),
                 // Click the save toggle and make sure the saved tab group
                 // appears in the bookmarks bar.
                 PressButton(kTabGroupEditorBubbleSaveToggleId));
  }

  MultiStep SaveGroupAndCloseEditorBubble(tab_groups::TabGroupId group_id) {
    return Steps(SaveGroupLeaveEditorBubbleOpen(group_id),
                 // Close the editor bubble view. Must flush events first to
                 // avoid closing a view while it's in the stack frame above us.
                 FlushEvents(), HoverTabGroupHeader(group_id), ClickMouse());
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest, CreateGroupAndSave) {
  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(ShowBookmarksBar(),
                  // Ensure no tab groups save buttons in the bookmarks bar
                  // are present.
                  EnsureNotPresent(kSavedTabGroupButtonElementId),
                  // Add tab at index 0 to a new group and save it.
                  SaveGroupLeaveEditorBubbleOpen(group_id),
                  WaitForShow(kSavedTabGroupButtonElementId, true));
}

// TODO(crbug.com/1440199): Re-enable this test
#if BUILDFLAG(IS_WIN)
#define MAYBE_UnsaveGroupFromTabGroupHeader \
  DISABLED_UnsaveGroupFromTabGroupHeader
#else
#define MAYBE_UnsaveGroupFromTabGroupHeader UnsaveGroupFromTabGroupHeader
#endif
IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest,
                       MAYBE_UnsaveGroupFromTabGroupHeader) {
  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      ShowBookmarksBar(),
      // Ensure no tab groups save buttons in the bookmarks bar are present.
      EnsureNotPresent(kSavedTabGroupButtonElementId),
      SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kSavedTabGroupButtonElementId, true),
      // Click the save toggle again and make sure the saved tab group
      // disappears from the bookmarks bar.
      PressButton(kTabGroupEditorBubbleSaveToggleId),
      WaitForHide(kSavedTabGroupButtonElementId),
      // Click the first tab to close the context menu. Mac builders fail if the
      // context menu stays open.
      HoverTabAt(0), ClickMouse(ui_controls::LEFT));
}

// TODO(crbug.com/1432770): Re-enable this test once it doesn't get stuck in
// drag and drop. Maybe related issue - the relative positioning seems to be
// interpreted as an absolute position.
IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest,
                       DISABLED_DragGroupWithinBar) {
  // Create two tab groups with one tab each.
  const tab_groups::TabGroupId group_id_1 =
      browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->InsertWebContentsAt(1, CreateWebContents(),
                                                    AddTabTypes::ADD_NONE);
  const tab_groups::TabGroupId group_id_2 =
      browser()->tab_strip_model()->AddToNewGroup({1});
  BrowserView::GetBrowserViewForBrowser(browser())->tabstrip()->StopAnimating(
      true);

  const char kSavedTabGroupButton1[] = "SavedTabGroupButton1";
  const char kSavedTabGroupButton2[] = "SavedTabGroupButton2";
  auto right_center =
      base::BindLambdaForTesting([](ui::TrackedElement* element) {
        return element->AsA<views::TrackedElementViews>()
            ->view()
            ->GetLocalBounds()
            .right_center();
      });

  RunTestSequence(
      // This comment fixes the auto formatting, do not remove.
      ShowBookmarksBar(),
      // Save the groups.
      SaveGroupAndCloseEditorBubble(group_id_1),
      SaveGroupAndCloseEditorBubble(group_id_2),
      // Find the buttons in the saved tab groups bar.
      NameChildViewByType<SavedTabGroupButton>(kSavedTabGroupBarElementId,
                                               kSavedTabGroupButton1, 0),
      NameChildViewByType<SavedTabGroupButton>(kSavedTabGroupBarElementId,
                                               kSavedTabGroupButton2, 1),
      // Drag button 1 to the right of button 2.
      MoveMouseTo(kSavedTabGroupButton1),
      DragMouseTo(kSavedTabGroupButton2, std::move(right_center)));

  SavedTabGroupModel* model =
      SavedTabGroupServiceFactory::GetForProfile(browser()->profile())->model();
  EXPECT_EQ(1, model->GetIndexOf(group_id_1).value());
}

// TODO(dljames): Write a test to unsave a group from a saved group button's
// context menu in the bookmarks bar.
