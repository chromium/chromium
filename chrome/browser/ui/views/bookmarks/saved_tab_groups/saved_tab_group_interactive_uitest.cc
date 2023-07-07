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
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
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
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"
#include "url/url_constants.h"

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

  StepBuilder CheckIfSavedGroupIsOpen(const base::Uuid* const saved_guid) {
    return Do([=]() {
      const SavedTabGroupKeyedService* const service =
          SavedTabGroupServiceFactory::GetForProfile(browser()->profile());

      const SavedTabGroup* const group = service->model()->Get(*saved_guid);
      ASSERT_NE(nullptr, group);
      EXPECT_TRUE(group->local_group_id().has_value());
      EXPECT_TRUE(browser()->tab_strip_model()->group_model()->ContainsTabGroup(
          group->local_group_id().value()));
    });
  }

  StepBuilder CheckIfSavedGroupIsClosed(const base::Uuid* const saved_guid) {
    return Do([=]() {
      const SavedTabGroupKeyedService* const service =
          SavedTabGroupServiceFactory::GetForProfile(browser()->profile());

      EXPECT_EQ(1, service->model()->Count());

      const SavedTabGroup* const group = service->model()->Get(*saved_guid);
      ASSERT_NE(nullptr, group);
      EXPECT_FALSE(group->local_group_id().has_value());
    });
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

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure no tab groups save buttons in the bookmarks bar
      // are present.
      EnsureNotPresent(kSavedTabGroupButtonElementId),
      // Add tab at index 0 to a new group and save it.
      SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kSavedTabGroupButtonElementId, true));
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest,
                       UnsaveGroupFromTabGroupHeader) {
  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
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
      FlushEvents(), HoverTabGroupHeader(group_id),
      ClickMouse(ui_controls::LEFT));
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest,
                       UnsaveGroupFromButtonMenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure no tab groups save buttons in the bookmarks bar are present.
      EnsureNotPresent(kSavedTabGroupButtonElementId),
      SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kSavedTabGroupButtonElementId, true),
      // Click the tab group header to close the menu.
      FlushEvents(), HoverTabGroupHeader(group_id),
      ClickMouse(ui_controls::LEFT), FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      WithElement(kSavedTabGroupButtonElementId,
                  [](ui::TrackedElement* el) {
                    const ui::KeyEvent event(
                        ui::ET_KEY_PRESSED, ui::KeyboardCode::VKEY_RETURN,
                        ui::DomCode::ENTER, ui::EF_NONE, ui::DomKey::ENTER,
                        base::TimeTicks(), /*is_char=*/false);

                    AsView<SavedTabGroupButton>(el)->OnKeyPressed(event);
                  }),
      // Flush events and select the delete group menu item.
      EnsurePresent(SavedTabGroupButton::kDeleteGroupMenuItem), FlushEvents(),
      SelectMenuItem(SavedTabGroupButton::kDeleteGroupMenuItem),
      // Ensure the button is no longer present.
      FinishTabstripAnimations(), WaitForHide(kSavedTabGroupButtonElementId));
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest,
                       FirstTabIsFocusedInReopenedSavedGroup) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Add 2 tabs to the group.
  const tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  const SavedTabGroupKeyedService* const service =
      SavedTabGroupServiceFactory::GetForProfile(browser()->profile());

  base::Uuid saved_guid;

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure no saved group buttons in the bookmarks bar are present.
      EnsureNotPresent(kSavedTabGroupButtonElementId),
      // Save the group and ensure it is linked in the model.
      SaveGroupLeaveEditorBubbleOpen(local_group_id),
      WaitForShow(kSavedTabGroupButtonElementId, true),
      // The group we just saved should be the only group in the model.
      CheckResult([&]() { return service->model()->Count(); }, 1),
      // Find the saved guid that is linked to the group we just saved.
      Do([&]() {
        const SavedTabGroup& saved_group =
            service->model()->saved_tab_groups()[0];
        ASSERT_TRUE(saved_group.local_group_id().has_value());
        saved_guid = saved_group.saved_guid();
      }),
      // Make sure the editor bubble is still open and flush events before we
      // close it.
      EnsurePresent(kTabGroupEditorBubbleId), FlushEvents(),
      // Close the tab group and expect the saved group is no longer linked.
      PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
      FinishTabstripAnimations(), CheckIfSavedGroupIsClosed(&saved_guid),
      // Reopen the tab group and expect the saved group is linked again.
      PressButton(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),
      CheckIfSavedGroupIsOpen(&saved_guid),
      // Verify the first tab in the group is the active tab.
      CheckResult(
          [&]() { return browser()->tab_strip_model()->active_index(); }, 1));
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest,
                       UpdateButtonWhenTabGroupVisualDataChanges) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  const std::u16string new_title = u"New title";
  const tab_groups::TabGroupColorId new_color =
      tab_groups::TabGroupColorId::kPurple;

  TabGroup* const group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
  const tab_groups::TabGroupVisualData* const old_visual_data =
      group->visual_data();

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure no tab groups save buttons in the bookmarks bar are present.
      EnsureNotPresent(kSavedTabGroupButtonElementId),
      SaveGroupLeaveEditorBubbleOpen(group_id),
      WaitForShow(kSavedTabGroupButtonElementId, true),
      // Verify the button in the bookmarks bar has the same color and title
      // as the tab group.
      CheckViewProperty(kSavedTabGroupButtonElementId,
                        &SavedTabGroupButton::GetText,
                        old_visual_data->title()),
      CheckViewProperty(kSavedTabGroupButtonElementId,
                        &SavedTabGroupButton::tab_group_color_id,
                        old_visual_data->color()),
      // Update the text and color.
      Do([&]() {
        group->SetVisualData(/*visual_data=*/{new_title, new_color});
      }),
      // Verify the button has the same color and title as the tab group.
      CheckViewProperty(kSavedTabGroupButtonElementId,
                        &SavedTabGroupButton::GetText, new_title),
      CheckViewProperty(kSavedTabGroupButtonElementId,
                        &SavedTabGroupButton::tab_group_color_id, new_color),
      // Click the tab group header to close the menu.
      FlushEvents(), HoverTabGroupHeader(group_id),
      ClickMouse(ui_controls::LEFT), FinishTabstripAnimations());
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupInteractiveTest,
                       FiveSavedGroupsShowsOverflowMenuButton) {
  // Add 4 additional tabs to the browser.
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(5, browser()->tab_strip_model()->count());

  // Add each tab to a separate group.
  const tab_groups::TabGroupId group_1 =
      browser()->tab_strip_model()->AddToNewGroup({0});
  const tab_groups::TabGroupId group_2 =
      browser()->tab_strip_model()->AddToNewGroup({1});
  const tab_groups::TabGroupId group_3 =
      browser()->tab_strip_model()->AddToNewGroup({2});
  const tab_groups::TabGroupId group_4 =
      browser()->tab_strip_model()->AddToNewGroup({3});
  const tab_groups::TabGroupId group_5 =
      browser()->tab_strip_model()->AddToNewGroup({4});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure no saved group buttons in the bookmarks bar are present.
      EnsureNotPresent(kSavedTabGroupButtonElementId),

      // Verify the overflow button is hidden until the 5th group is saved.
      SaveGroupAndCloseEditorBubble(group_1), FinishTabstripAnimations(),
      SaveGroupAndCloseEditorBubble(group_2), FinishTabstripAnimations(),
      SaveGroupAndCloseEditorBubble(group_3), FinishTabstripAnimations(),
      SaveGroupAndCloseEditorBubble(group_4), FinishTabstripAnimations(),
      EnsureNotPresent(kSavedTabGroupOverflowButtonElementId),
      SaveGroupAndCloseEditorBubble(group_5), FinishTabstripAnimations(),
      EnsurePresent(kSavedTabGroupOverflowButtonElementId),

      // Verify there is only 1 button in the overflow menu
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForShow(kSavedTabGroupOverflowMenuId, true),
      CheckView(kSavedTabGroupOverflowMenuId,
                [](views::View* el) { return el->children().size() == 1u; }),
      // Hide the overflow menu.
      FlushEvents(),
      SendAccelerator(
          kSavedTabGroupOverflowMenuId,
          ui::Accelerator(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForHide(kSavedTabGroupOverflowMenuId));
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
      FinishTabstripAnimations(), ShowBookmarksBar(),
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
