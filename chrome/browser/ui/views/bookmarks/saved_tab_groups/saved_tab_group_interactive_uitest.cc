// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
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
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_utils.h"
#include "url/url_constants.h"

namespace tab_groups {

class SavedTabGroupInteractiveTestBase : public InteractiveBrowserTest {
 public:
  SavedTabGroupInteractiveTestBase() = default;
  ~SavedTabGroupInteractiveTestBase() override = default;

  MultiStep ShowBookmarksBar() {
    return Steps(PressButton(kToolbarAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }

  MultiStep FinishTabstripAnimations() {
    return Steps(
        WaitForShow(kTabStripElementId),
        std::move(WithView(kTabStripElementId, [](TabStrip* tab_strip) {
                    tab_strip->StopAnimating(true);
                  }).SetDescription("FinishTabstripAnimation")));
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
};

class SavedTabGroupInteractiveTest
    : public SavedTabGroupInteractiveTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  SavedTabGroupInteractiveTest() = default;
  ~SavedTabGroupInteractiveTest() override = default;

  void SetUp() override {
    if (IsV2UIEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {tab_groups::kTabGroupsSaveUIUpdate, tab_groups::kTabGroupsSaveV2,
           tab_groups::kTabGroupSyncServiceDesktopMigration},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {tab_groups::kTabGroupsSaveV2},
          {tab_groups::kTabGroupsSaveUIUpdate,
           tab_groups::kTabGroupSyncServiceDesktopMigration});
    }

    SavedTabGroupInteractiveTestBase::SetUp();
  }

  bool IsV2UIEnabled() const { return GetParam(); }

  MultiStep HoverTabAt(int index) {
    const char kTabToHover[] = "Tab to hover";
    return Steps(NameDescendantViewByType<Tab>(kBrowserViewElementId,
                                               kTabToHover, index),
                 MoveMouseTo(kTabToHover));
  }

  MultiStep OpenTabGroupEditorMenu(tab_groups::TabGroupId group_id) {
    return Steps(HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
                 WaitForShow(kTabGroupEditorBubbleId));
  }

  StepBuilder CheckIfSavedGroupIsOpen(const base::Uuid* const saved_guid) {
    return Do([=, this]() {
      const std::optional<SavedTabGroup> group =
          service()->GetGroup(*saved_guid);
      ASSERT_TRUE(group);
      EXPECT_TRUE(group->local_group_id());
      EXPECT_TRUE(browser()->tab_strip_model()->group_model()->ContainsTabGroup(
          group->local_group_id().value()));
    });
  }

  StepBuilder CheckIfSavedGroupIsClosed(const base::Uuid* const saved_guid) {
    return Do([=, this]() {
      EXPECT_EQ(1u, service()->GetAllGroups().size());
      const std::optional<SavedTabGroup> group =
          service()->GetGroup(*saved_guid);
      ASSERT_TRUE(group);
      EXPECT_FALSE(group->local_group_id());
    });
  }

  StepBuilder CheckIfSavedGroupIsPinned(tab_groups::TabGroupId group_id,
                                        bool is_pinned) {
    return Do([=, this]() {
      EXPECT_EQ(is_pinned, service()->GetGroup(group_id)->is_pinned());
    });
  }

  StepBuilder SaveGroupViaModel(const tab_groups::TabGroupId local_group) {
    return Do([=, this]() {
      service()->AddGroup(
          SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(local_group));
      ASSERT_TRUE(service()->GetGroup(local_group).has_value());
    });
  }

  StepBuilder CreateEmptySavedGroup() {
    return Do([=, this]() {
      TabGroupSyncService* service =
          SavedTabGroupUtils::GetServiceForProfile(browser()->profile());
      service->AddGroup({u"Test Test",
                         tab_groups::TabGroupColorId::kBlue,
                         {},
                         /*position=*/std::nullopt,
                         base::Uuid::GenerateRandomV4(),
                         /*local_group_id=*/std::nullopt,
                         /*creator_cache_guid=*/std::nullopt,
                         /*last_updater_cache_guid=*/std::nullopt,
                         /*created_before_syncing_tab_groups=*/false,
                         /*creation_time_windows_epoch_micros=*/std::nullopt});
    });
  }

  StepBuilder UnsaveGroupViaModel(const tab_groups::TabGroupId local_group) {
    return Do([=, this]() {
      service()->RemoveGroup(local_group);
      ASSERT_FALSE(service()->GetGroup(local_group).has_value());
    });
  }

  auto CheckEverythingButtonVisibility(bool is_v2_enabled) {
    return is_v2_enabled
               ? EnsurePresent(kSavedTabGroupOverflowButtonElementId)
               : EnsureNotPresent(kSavedTabGroupOverflowButtonElementId);
  }

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContents::Create(
        content::WebContents::CreateParams(browser()->profile()));
  }

  TabGroupSyncService* service() {
    return SavedTabGroupUtils::GetServiceForProfile(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Used to test SavedTabGroups V2 specific features such as the save toggle.
class SavedTabGroupInteractiveTestV1
    : public SavedTabGroupInteractiveTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  SavedTabGroupInteractiveTestV1() = default;
  ~SavedTabGroupInteractiveTestV1() override = default;

  void SetUp() override {
    if (IsV2UIEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {tab_groups::kTabGroupSyncServiceDesktopMigration},
          {tab_groups::kTabGroupsSaveV2});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {tab_groups::kTabGroupSyncServiceDesktopMigration,
               tab_groups::kTabGroupsSaveV2});
    }

    SavedTabGroupInteractiveTestBase::SetUp();
  }

  bool IsV2UIEnabled() const { return GetParam(); }

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTestV1,
                       ToggleSavedStateOfGroup) {
  const tab_groups::TabGroupId& group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  RunTestSequence(FinishTabstripAnimations(), ShowBookmarksBar(),
                  // Verify pressing the toggle saves the group.
                  SaveGroupLeaveEditorBubbleOpen(group_id),
                  EnsurePresent(kSavedTabGroupButtonElementId),
                  // Verify pressing the toggle again unsaves the group.
                  PressButton(kTabGroupEditorBubbleSaveToggleId),
                  EnsureNotPresent(kSavedTabGroupButtonElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest, CreateGroupAndSave) {
  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       UnsaveGroupFromTabGroupHeader) {
  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId),
      OpenTabGroupEditorMenu(group_id),
      // Delete the group and verify the button is no longer present for it.
      PressButton(kTabGroupEditorBubbleDeleteGroupButtonId),
      // Accept the deletion dialog.
      Do([&]() {
        browser()
            ->tab_group_deletion_dialog_controller()
            ->SimulateOkButtonForTesting();
      }),
      EnsureNotPresent(kSavedTabGroupButtonElementId));
}

// TODO(crbug.com/368107327): Fix test failures on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_UnsaveGroupFromButtonMenu DISABLED_UnsaveGroupFromButtonMenu
#else
#define MAYBE_UnsaveGroupFromButtonMenu UnsaveGroupFromButtonMenu
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MAYBE_UnsaveGroupFromButtonMenu) {
  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      WithElement(kSavedTabGroupButtonElementId,
                  [](ui::TrackedElement* el) {
                    const ui::KeyEvent event(
                        ui::EventType::kKeyPressed,
                        ui::KeyboardCode::VKEY_RETURN, ui::DomCode::ENTER,
                        ui::EF_NONE, ui::DomKey::ENTER, base::TimeTicks(),
                        /*is_char=*/false);

                    AsView<SavedTabGroupButton>(el)->OnKeyPressed(event);
                  }),
      // Flush events and select the delete group menu item.
      EnsurePresent(SavedTabGroupUtils::kDeleteGroupMenuItem),
      SelectMenuItem(SavedTabGroupUtils::kDeleteGroupMenuItem),
      // Accept the deletion dialog.
      Do([&]() {
        browser()
            ->tab_group_deletion_dialog_controller()
            ->SimulateOkButtonForTesting();
      }),
      // Ensure the button is no longer present.
      EnsureNotPresent(kSavedTabGroupButtonElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest, UnpinGroupFromButtonMenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

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
      // Ensure the group was saved when created.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // Ensure the tab group is pinned.
      CheckIfSavedGroupIsPinned(group_id, /*is_pinned=*/true),
      FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      WithElement(kSavedTabGroupButtonElementId,
                  [](ui::TrackedElement* el) {
                    const ui::KeyEvent event(
                        ui::EventType::kKeyPressed,
                        ui::KeyboardCode::VKEY_RETURN, ui::DomCode::ENTER,
                        ui::EF_NONE, ui::DomKey::ENTER, base::TimeTicks(),
                        /*is_char=*/false);

                    AsView<SavedTabGroupButton>(el)->OnKeyPressed(event);
                  }),
      // Flush events and select the unpin group menu item.
      EnsurePresent(SavedTabGroupUtils::kToggleGroupPinStateMenuItem),

      SelectMenuItem(SavedTabGroupUtils::kToggleGroupPinStateMenuItem),
      FinishTabstripAnimations(), WaitForHide(kSavedTabGroupButtonElementId),
      // Ensure the tab group is unpinned.
      CheckIfSavedGroupIsPinned(group_id, /*is_pinned=*/false));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       ContextMenuShowForEverythingMenuTabGroupItem) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForShow(STGEverythingMenu::kTabGroup),
      WithElement(
          STGEverythingMenu::kTabGroup,
          [](ui::TrackedElement* el) {
            ui::test::EventGenerator event_generator(
                views::GetRootWindow(AsView<views::View>(el)->GetWidget()));
            event_generator.MoveMouseTo(
                AsView<views::View>(el)->GetBoundsInScreen().CenterPoint());
            event_generator.ClickRightButton();
          }),
      EnsurePresent(SavedTabGroupUtils::kMoveGroupToNewWindowMenuItem),
      EnsurePresent(SavedTabGroupUtils::kToggleGroupPinStateMenuItem),
      EnsurePresent(SavedTabGroupUtils::kDeleteGroupMenuItem),
      EnsurePresent(SavedTabGroupUtils::kTabsTitleItem));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       SubmenuShowForAppMenuTabGroups) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

      browser()->tab_strip_model()->AddToNewGroup({0});

      RunTestSequence(
          FinishTabstripAnimations(), ShowBookmarksBar(),
          PressButton(kToolbarAppMenuButtonElementId),
          WaitForShow(AppMenuModel::kTabGroupsMenuItem),
          SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
          SelectMenuItem(STGEverythingMenu::kTabGroup),
          EnsurePresent(STGEverythingMenu::kOpenGroup),
          EnsurePresent(SavedTabGroupUtils::kMoveGroupToNewWindowMenuItem),
          EnsurePresent(SavedTabGroupUtils::kToggleGroupPinStateMenuItem),
          EnsurePresent(SavedTabGroupUtils::kDeleteGroupMenuItem),
          EnsurePresent(SavedTabGroupUtils::kTabsTitleItem),
          EnsurePresent(SavedTabGroupUtils::kTab));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       OpenGroupFromAppMenuTabGroupSubmenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Add 2 tabs to the group.
  const tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  base::Uuid saved_guid;

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure saved group buttons in the bookmarks bar are present.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // The group we just saved should be the only group in the model.
      CheckResult([&]() { return service()->GetAllGroups().size(); }, 1u),
      // Find the saved guid that is linked to the group we just saved.
      Do([&]() {
        const std::optional<SavedTabGroup> saved_group =
            service()->GetGroup(local_group_id);
        ASSERT_TRUE(saved_group);
        saved_guid = saved_group->saved_guid();
      }),
      // Open the editor bubble, close thegroup and expect the saved group is no
      // longer linked.
      OpenTabGroupEditorMenu(local_group_id),
      PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
      FinishTabstripAnimations(), CheckIfSavedGroupIsClosed(&saved_guid),
      // Reopen the tab group from the app menu tab group's submenu.
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      SelectMenuItem(STGEverythingMenu::kOpenGroup), FinishTabstripAnimations(),
      WaitForShow(kTabGroupHeaderElementId));
}

// TODO(crbug.com/368107327): Fix test failures on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MoveGroupToNewWindowFromAppMenuTabGroupSubmenu \
  DISABLED_MoveGroupToNewWindowFromAppMenuTabGroupSubmenu
#else
#define MAYBE_MoveGroupToNewWindowFromAppMenuTabGroupSubmenu \
  MoveGroupToNewWindowFromAppMenuTabGroupSubmenu
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MAYBE_MoveGroupToNewWindowFromAppMenuTabGroupSubmenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      SelectMenuItem(SavedTabGroupUtils::kMoveGroupToNewWindowMenuItem),
      FinishTabstripAnimations(),
      // Expect the original browser has 1 less tab.
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      // Expect the browser with the tab group is not the original browser.
      CheckResult(
          [&]() {
            return browser() ==
                   SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
          },
          false));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       UnpinGroupFromAppMenuTabGroupSubmenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the group was saved into the bookmarks bar.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // Ensure the tab group is pinned.
      CheckIfSavedGroupIsPinned(group_id, /*is_pinned=*/true),
      FinishTabstripAnimations(), PressButton(kToolbarAppMenuButtonElementId),
      WaitForShow(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      SelectMenuItem(SavedTabGroupUtils::kToggleGroupPinStateMenuItem),
      WaitForHide(kSavedTabGroupButtonElementId),
      CheckIfSavedGroupIsPinned(group_id, /*is_pinned=*/false));
}

// TODO(crbug.com/368107327): Fix test failures on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DeleteGroupFromAppMenuTabGroupSubmenu \
  DISABLED_DeleteGroupFromAppMenuTabGroupSubmenu
#else
#define MAYBE_DeleteGroupFromAppMenuTabGroupSubmenu \
  DeleteGroupFromAppMenuTabGroupSubmenu
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MAYBE_DeleteGroupFromAppMenuTabGroupSubmenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      // Selecting the menu item will spanw a dialog instead
      SelectMenuItem(SavedTabGroupUtils::kDeleteGroupMenuItem), Do([&]() {
        browser()
            ->tab_group_deletion_dialog_controller()
            ->SimulateOkButtonForTesting();
      }),
      FinishTabstripAnimations(),
      // Expect the saved tab group button disappears.
      WaitForHide(kSavedTabGroupButtonElementId),
      // Expect the original browser has 1 less tab.
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      // Expect the browser has no such a tab group.
      CheckResult(
          [&]() {
            return nullptr ==
                   SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
          },
          true));
}

// TODO(crbug.com/368107327): Fix test failures on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenTabFromAppMenuTabGroupSubmenu \
  DISABLED_OpenTabFromAppMenuTabGroupSubmenu
#else
#define MAYBE_OpenTabFromAppMenuTabGroupSubmenu \
  OpenTabFromAppMenuTabGroupSubmenu
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MAYBE_OpenTabFromAppMenuTabGroupSubmenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  // Add 1 tab into the browser. And verify there are 2 tabs (The tab when you
  // open the browser and the added one).
  GURL test_url(chrome::kChromeUINewTabURL);
  ASSERT_TRUE(AddTabAtIndex(0, test_url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kTabGroup),
      SelectMenuItem(SavedTabGroupUtils::kTab),
      WaitForHide(AppMenuModel::kTabGroupsMenuItem), FinishTabstripAnimations(),

      // Expect the original browser has 1 more tab.
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 3),
      // Expect the active tab is the one opened from submenu.
      CheckResult(
          [&]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetURL();
          },
          test_url),
      // Expect the active tab is at the end of tab strip.
      CheckResult(
          [&]() { return browser()->tab_strip_model()->active_index(); }, 2));
}

// TODO(crbug.com/368107327): Fix test failures on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MoveGroupToNewWindowFromButtonMenu \
  DISABLED_MoveGroupToNewWindowFromButtonMenu
#else
#define MAYBE_MoveGroupToNewWindowFromButtonMenu \
  MoveGroupToNewWindowFromButtonMenu
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MAYBE_MoveGroupToNewWindowFromButtonMenu) {
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
      // Ensure the tab group was saved into the bookmarks bar.
      EnsurePresent(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      WithElement(kSavedTabGroupButtonElementId,
                  [](ui::TrackedElement* el) {
                    const ui::KeyEvent event(
                        ui::EventType::kKeyPressed,
                        ui::KeyboardCode::VKEY_RETURN, ui::DomCode::ENTER,
                        ui::EF_NONE, ui::DomKey::ENTER, base::TimeTicks(),
                        /*is_char=*/false);

                    AsView<SavedTabGroupButton>(el)->OnKeyPressed(event);
                  }),
      // Flush events and select the move group to new window menu item.
      EnsurePresent(SavedTabGroupUtils::kMoveGroupToNewWindowMenuItem),

      SelectMenuItem(SavedTabGroupUtils::kMoveGroupToNewWindowMenuItem),
      // Ensure the button is no longer present.
      FinishTabstripAnimations(),
      // Expect the original browser has 1 less tab.
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      // Expect the browser with the tab group is not the original browser.
      CheckResult(
          [&]() {
            return browser() ==
                   SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
          },
          false));
}

IN_PROC_BROWSER_TEST_P(
    SavedTabGroupInteractiveTest,
    MoveGroupToNewWindowFromButtonMenuDoesNothingIfOnlyGroupInWindow) {
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the tab group was saved.
      EnsurePresent(kSavedTabGroupButtonElementId), FinishTabstripAnimations(),
      // Press the enter/return key on the button to open the context menu.
      WithElement(kSavedTabGroupButtonElementId,
                  [](ui::TrackedElement* el) {
                    const ui::KeyEvent event(
                        ui::EventType::kKeyPressed,
                        ui::KeyboardCode::VKEY_RETURN, ui::DomCode::ENTER,
                        ui::EF_NONE, ui::DomKey::ENTER, base::TimeTicks(),
                        /*is_char=*/false);

                    AsView<SavedTabGroupButton>(el)->OnKeyPressed(event);
                  }),
      // Flush events and select the move group to new window menu item.
      EnsurePresent(SavedTabGroupUtils::kMoveGroupToNewWindowMenuItem),

      SelectMenuItem(SavedTabGroupUtils::kMoveGroupToNewWindowMenuItem),
      // Ensure the button is no longer present.
      FinishTabstripAnimations(),
      // Expect the original browser has 1 less tab.
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      // Expect the browser with the tab group is the original browser.
      CheckResult(
          [&]() {
            return browser() ==
                   SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
          },
          true));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       FirstTabIsFocusedInReopenedSavedGroup) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  base::Uuid saved_guid = service()->GetGroup(local_group_id)->saved_guid();

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure no saved group buttons in the bookmarks bar are present.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // The group we just saved should be the only group in the model.
      CheckResult([&]() { return service()->GetAllGroups().size(); }, 1),
      // Open the editor bubble and close the group.
      FinishTabstripAnimations(), OpenTabGroupEditorMenu(local_group_id),
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

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MoveTabInsideAndOutsideGroup) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Add 2 tabs to the group.
  const tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(), Do([&]() {
        browser()->tab_strip_model()->MoveWebContentsAt(1, 2, false);
      }),
      CheckResult(
          [&]() { return browser()->tab_strip_model()->GetTabGroupForTab(2); },
          std::nullopt),
      Do([&]() {
        std::vector<int> indices = {2};
        browser()->tab_strip_model()->AddToExistingGroup(indices,
                                                         local_group_id);
      }),
      CheckResult(
          [&]() { return browser()->tab_strip_model()->GetTabGroupForTab(1); },
          local_group_id));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
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
      // Ensure the button was added to tbe bookmarks bar.
      EnsurePresent(kSavedTabGroupButtonElementId),
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
      HoverTabGroupHeader(group_id), ClickMouse(ui_controls::LEFT),
      FinishTabstripAnimations());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       CreateNewTabGroupFromEverythingMenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }
  const bool is_v2_ui_enabled = IsV2UIEnabled();

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      CheckEverythingButtonVisibility(is_v2_ui_enabled),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      EnsureNotPresent(kTabGroupEditorBubbleId),
      PressButton(kSavedTabGroupOverflowButtonElementId),
      EnsurePresent(STGEverythingMenu::kCreateNewTabGroup),
      SelectMenuItem(STGEverythingMenu::kCreateNewTabGroup),
      FinishTabstripAnimations(), WaitForShow(kTabGroupEditorBubbleId),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2),
      // This menu item opens a new tab and the editor bubble.
      CheckResult(
          [&]() { return browser()->tab_strip_model()->active_index(); }, 1),
      CheckResult(
          [&]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .host_piece();
          },
          chrome::kChromeUINewTabHost));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       OpenSavedGroupFromEverythingMenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Add 2 tabs to the group.
  const tab_groups::TabGroupId local_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  base::Uuid saved_guid;

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // The group we just saved should be the only group in the model.
      CheckResult([&]() { return service()->GetAllGroups().size(); }, 1),
      // Find the saved guid that is linked to the group we just saved.
      Do([&]() {
        const std::optional<SavedTabGroup> saved_group =
            service()->GetGroup(local_group_id);
        ASSERT_TRUE(saved_group);
        saved_guid = saved_group->saved_guid();
      }),
      // Open the tab group editor bubble.
      OpenTabGroupEditorMenu(local_group_id),
      // Close the tab group and expect the saved group is no longer linked.
      PressButton(kTabGroupEditorBubbleCloseGroupButtonId),
      FinishTabstripAnimations(), CheckIfSavedGroupIsClosed(&saved_guid),
      // Open the saved tab group from the Everything menu item.
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForHide(kTabGroupEditorBubbleId),
      SelectMenuItem(STGEverythingMenu::kTabGroup), FinishTabstripAnimations(),
      WaitForShow(kTabGroupHeaderElementId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       CreateNewTabGroupFromAppMenuSubmenu) {
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }
  const bool is_v2_ui_enabled = IsV2UIEnabled();

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      CheckEverythingButtonVisibility(is_v2_ui_enabled),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 1),
      EnsureNotPresent(kTabGroupEditorBubbleId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(STGEverythingMenu::kCreateNewTabGroup),
      FinishTabstripAnimations(), WaitForShow(kTabGroupEditorBubbleId),
      CheckResult([&]() { return browser()->tab_strip_model()->count(); }, 2),
      // This menu item opens a new tab and the editor bubble.
      CheckResult(
          [&]() { return browser()->tab_strip_model()->active_index(); }, 1),
      CheckResult(
          [&]() {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL()
                .host_piece();
          },
          chrome::kChromeUINewTabHost));
}

// TODO(crbug.com/368107327): Fix test failures on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_EverythingButtonAlwaysShowsForV2 \
  DISABLED_EverythingButtonAlwaysShowsForV2
#else
#define MAYBE_EverythingButtonAlwaysShowsForV2 EverythingButtonAlwaysShowsForV2
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       MAYBE_EverythingButtonAlwaysShowsForV2) {
  browser()->tab_strip_model()->AddToNewGroup({0});
  const bool is_v2_ui_enabled = IsV2UIEnabled();

  RunTestSequence(
      FinishTabstripAnimations(), ShowBookmarksBar(),
      CheckEverythingButtonVisibility(is_v2_ui_enabled),
      // Press the enter/return key on the button to open the context menu.
      WithElement(kSavedTabGroupButtonElementId,
                  [](ui::TrackedElement* el) {
                    const ui::KeyEvent event(
                        ui::EventType::kKeyPressed,
                        ui::KeyboardCode::VKEY_RETURN, ui::DomCode::ENTER,
                        ui::EF_NONE, ui::DomKey::ENTER, base::TimeTicks(),
                        /*is_char=*/false);

                    AsView<SavedTabGroupButton>(el)->OnKeyPressed(event);
                  }),
      // Flush events and select the delete group menu item.
      EnsurePresent(SavedTabGroupUtils::kDeleteGroupMenuItem),
      SelectMenuItem(SavedTabGroupUtils::kDeleteGroupMenuItem),
      // Accept the deletion dialog.
      Do([&]() {
        browser()
            ->tab_group_deletion_dialog_controller()
            ->SimulateOkButtonForTesting();
      }),
      // Ensure the button is no longer present.
      EnsureNotPresent(kSavedTabGroupButtonElementId),
      CheckEverythingButtonVisibility(is_v2_ui_enabled));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       FiveSavedGroupsShowsOverflowMenuButton) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }
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
  browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->AddToNewGroup({1});
  browser()->tab_strip_model()->AddToNewGroup({2});
  browser()->tab_strip_model()->AddToNewGroup({3});
  browser()->tab_strip_model()->AddToNewGroup({4});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the buttons are in the bookmarks bar.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // Verify the overflow button is shown.
      EnsurePresent(kSavedTabGroupOverflowButtonElementId),
      FinishTabstripAnimations(),

      // Verify there is only 1 button in the overflow menu
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForShow(kSavedTabGroupOverflowMenuId, true),
      CheckView(kSavedTabGroupOverflowMenuId,
                [](views::View* el) { return el->children().size() == 1u; }),
      // Hide the overflow menu.
      SendAccelerator(
          kSavedTabGroupOverflowMenuId,
          ui::Accelerator(ui::KeyboardCode::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForHide(kSavedTabGroupOverflowMenuId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       FourSavedGroupsNoOverflowMenuButton) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }
  // Add 4 additional tabs to the browser.
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  ASSERT_EQ(4, browser()->tab_strip_model()->count());

  // Add each tab to a separate group.
  browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->AddToNewGroup({1});
  browser()->tab_strip_model()->AddToNewGroup({2});
  browser()->tab_strip_model()->AddToNewGroup({3});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the buttons are in the bookmarks bar.
      EnsurePresent(kSavedTabGroupButtonElementId),
      // Verify the overflow button is shown.
      EnsureNotPresent(kSavedTabGroupOverflowButtonElementId));
}

// TODO(crbug.com/40264110): Re-enable this test once it doesn't get stuck in
// drag and drop. Maybe related issue - the relative positioning seems to be
// interpreted as an absolute position.
IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
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
      // Find the buttons in the saved tab groups bar.
      NameChildViewByType<SavedTabGroupButton>(kSavedTabGroupBarElementId,
                                               kSavedTabGroupButton1, 0),
      NameChildViewByType<SavedTabGroupButton>(kSavedTabGroupBarElementId,
                                               kSavedTabGroupButton2, 1),
      // Drag button 1 to the right of button 2.
      MoveMouseTo(kSavedTabGroupButton1),
      DragMouseTo(kSavedTabGroupButton2, std::move(right_center)));

  EXPECT_EQ(1u, service()->GetGroup(group_id_1)->position());
  EXPECT_EQ(0u, service()->GetGroup(group_id_2)->position());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       OverflowMenuClosesWhenNoMoreButtons) {
  if (IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  // Add 5 additional tabs to the browser.
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
  browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->AddToNewGroup({1});
  browser()->tab_strip_model()->AddToNewGroup({2});
  browser()->tab_strip_model()->AddToNewGroup({3});
  const tab_groups::TabGroupId group_5 =
      browser()->tab_strip_model()->AddToNewGroup({4});

  RunTestSequence(
      // Show the bookmarks bar where the buttons will be displayed.
      FinishTabstripAnimations(), ShowBookmarksBar(),
      // Ensure the buttons and the overflow menu are visible.
      EnsurePresent(kSavedTabGroupButtonElementId),
      EnsurePresent(kSavedTabGroupOverflowButtonElementId),

      // Show the overflow menu.
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForShow(kSavedTabGroupOverflowMenuId, true), Do([=, this]() {
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetWidget()
            ->LayoutRootViewIfNecessary();
      }),

      // Verify the overflow menu expands if another group is added.
      UnsaveGroupViaModel(group_5), Do([=, this]() {
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetWidget()
            ->LayoutRootViewIfNecessary();
      }),

      // Ensure the menu is no longer visible / present.
      WaitForHide(kSavedTabGroupOverflowMenuId),
      EnsureNotPresent(kSavedTabGroupOverflowMenuId));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       EverythingMenuDoesntDisplayEmptyGroups) {
  // The everything menu is only enabled in V2.
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  RunTestSequence(
      // Create an empty group
      FinishTabstripAnimations(), ShowBookmarksBar(), CreateEmptySavedGroup(),
      // Open the everything menu and expect the group to not show up.
      CheckEverythingButtonVisibility(IsV2UIEnabled()),
      PressButton(kSavedTabGroupOverflowButtonElementId),
      WaitForShow(STGEverythingMenu::kCreateNewTabGroup),
      EnsureNotPresent(STGEverythingMenu::kTabGroup));
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupInteractiveTest,
                       AppMenuDoesntDisplayEmptyGroups) {
  // The everything menu is only enabled in V2.
  if (!IsV2UIEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }

  RunTestSequence(
      // Create an empty group
      FinishTabstripAnimations(), ShowBookmarksBar(), CreateEmptySavedGroup(),
      // Open the app menu tab group menu.
      PressButton(kToolbarAppMenuButtonElementId),

      WaitForShow(AppMenuModel::kTabGroupsMenuItem),
      SelectMenuItem(AppMenuModel::kTabGroupsMenuItem),
      WaitForShow(STGEverythingMenu::kCreateNewTabGroup),
      // Expect the group to not be displayed.
      EnsureNotPresent(STGEverythingMenu::kTabGroup));
}

INSTANTIATE_TEST_SUITE_P(SavedTabGroupBar,
                         SavedTabGroupInteractiveTest,
                         testing::Bool());

INSTANTIATE_TEST_SUITE_P(SavedTabGroupBar,
                         SavedTabGroupInteractiveTestV1,
                         testing::Bool());
}  // namespace tab_groups
