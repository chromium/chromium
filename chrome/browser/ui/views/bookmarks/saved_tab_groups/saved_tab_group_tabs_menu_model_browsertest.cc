// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"  // For IDS_OPEN_GROUP_IN_BROWSER_MENU
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"  // For GetStringUTF16
#include "ui/base/page_transition_types.h"

namespace tab_groups {

class SavedTabGroupTabsMenuModelBrowserTest : public InProcessBrowserTest {
 public:
  SavedTabGroupTabsMenuModelBrowserTest() = default;

  int GetNextCommandId() { return next_command_id_++; }

  bool IsOpenGroupItemEnabled(STGTabsMenuModel* model) {
    std::u16string target_label =
        l10n_util::GetStringUTF16(IDS_OPEN_GROUP_IN_BROWSER_MENU);

    size_t item_count = model->GetItemCount();
    for (size_t i = 0; i < item_count; ++i) {
      if (model->GetLabelAt(i) == target_label) {
        return model->IsEnabledAt(i);
      }
    }
    return false;
  }

  bool IsCloseGroupItemEnabled(STGTabsMenuModel* model) {
    std::u16string target_label =
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP);

    size_t item_count = model->GetItemCount();
    for (size_t i = 0; i < item_count; ++i) {
      if (model->GetLabelAt(i) == target_label) {
        return model->IsEnabledAt(i);
      }
    }
    return false;
  }

  bool HasItemWithLabel(STGTabsMenuModel* model, const std::u16string& label) {
    size_t item_count = model->GetItemCount();
    for (size_t i = 0; i < item_count; ++i) {
      if (model->GetLabelAt(i) == label) {
        return true;
      }
    }
    return false;
  }

  bool IsPinItemPresent(STGTabsMenuModel* model) {
    std::u16string pin_label =
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP);
    std::u16string unpin_label =
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNPIN_GROUP);

    size_t item_count = model->GetItemCount();
    for (size_t i = 0; i < item_count; ++i) {
      if (model->GetLabelAt(i) == pin_label ||
          model->GetLabelAt(i) == unpin_label) {
        return true;
      }
    }
    return false;
  }

  std::optional<size_t> GetIndexOfElementId(STGTabsMenuModel* model,
                                            ui::ElementIdentifier id) {
    for (size_t i = 0; i < model->GetItemCount(); ++i) {
      if (model->GetElementIdentifierAt(i) == id) {
        return i;
      }
    }
    return std::nullopt;
  }

 protected:
  TabGroupSyncService* GetSyncService() {
    return TabGroupSyncServiceFactory::GetForProfile(browser()->GetProfile());
  }

 private:
  int next_command_id_ = 1;
};

// TEST 1: Verify "Open group" is ENABLED (and "Close group" not present) when
// the group is closed.
IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelBrowserTest,
                       OpenGroupEnabledWhenClosed) {
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  GetSyncService()->AddGroup(group);

  STGTabsMenuModel model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  EXPECT_TRUE(IsOpenGroupItemEnabled(&model));
  EXPECT_FALSE(HasItemWithLabel(
      &model,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP)));
  EXPECT_EQ(model.GetElementIdentifierAt(0), STGTabsMenuModel::kOpenGroup);
}

// TEST 2: Verify "Close group" is ENABLED (and "Open group" not present) when
// the group is open and expanded, and executing it closes the group.
IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelBrowserTest,
                       CloseGroupEnabledWhenOpenAndExpanded) {
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  TabStripModel* const tsm = browser()->GetTabStripModel();
  TabGroupId local_id = tsm->AddToNewGroup({0});

  tsm->ActivateTabAt(0);
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  group.SetLocalGroupId(local_id);
  GetSyncService()->AddGroup(group);

  STGTabsMenuModel model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  EXPECT_FALSE(HasItemWithLabel(
      &model, l10n_util::GetStringUTF16(IDS_OPEN_GROUP_IN_BROWSER_MENU)));
  EXPECT_TRUE(IsCloseGroupItemEnabled(&model));
  EXPECT_EQ(model.GetElementIdentifierAt(0), STGTabsMenuModel::kCloseGroup);

  EXPECT_TRUE(tsm->group_model()->ContainsTabGroup(local_id));
  model.ActivatedAt(0);
  EXPECT_FALSE(tsm->group_model()->ContainsTabGroup(local_id));
  EXPECT_EQ(user_action_tester.GetActionCount(
                "TabGroups_SavedTabGroups_TabGroupSubmenu_Closed"),
            1);
}

// TEST 3: Verify "Close group" works from the App Menu context.
IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelBrowserTest,
                       CloseGroupFromAppMenu) {
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  TabStripModel* const tsm = browser()->GetTabStripModel();
  TabGroupId local_id = tsm->AddToNewGroup({0});

  tsm->ActivateTabAt(0);
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  group.SetLocalGroupId(local_id);
  GetSyncService()->AddGroup(group);

  STGTabsMenuModel model(browser(), TabGroupMenuContext::APP_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  EXPECT_FALSE(HasItemWithLabel(
      &model, l10n_util::GetStringUTF16(IDS_OPEN_GROUP_IN_BROWSER_MENU)));
  EXPECT_TRUE(IsCloseGroupItemEnabled(&model));
  EXPECT_EQ(model.GetElementIdentifierAt(0), STGTabsMenuModel::kCloseGroup);

  EXPECT_TRUE(tsm->group_model()->ContainsTabGroup(local_id));
  model.ActivatedAt(0);
  EXPECT_FALSE(tsm->group_model()->ContainsTabGroup(local_id));
  EXPECT_EQ(user_action_tester.GetActionCount(
                "TabGroups_SavedTabGroups_TabGroupSubmenu_Closed"),
            1);
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelBrowserTest,
                       PinUnpinOptionVisibility) {
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  GetSyncService()->AddGroup(group);

  // Pin/unpin should be present for button context menu.
  STGTabsMenuModel button_model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  button_model.Build(
      group, base::BindRepeating(
                 &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                 base::Unretained(this)));

  EXPECT_TRUE(IsPinItemPresent(&button_model));

  // Pin/unpin should be hidden for organizer panel.
  STGTabsMenuModel organizer_model(browser(),
                                   TabGroupMenuContext::ORGANIZER_PANEL);
  organizer_model.Build(
      group, base::BindRepeating(
                 &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                 base::Unretained(this)));

  EXPECT_FALSE(IsPinItemPresent(&organizer_model));
}

class SavedTabGroupTabsMenuModelFocusingBrowserTest
    : public SavedTabGroupTabsMenuModelBrowserTest {
 public:
  SavedTabGroupTabsMenuModelFocusingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kTabGroupsFocusing);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelFocusingBrowserTest,
                       ToggleFocusGroupFromBookmarksBarContextMenu) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  TabStripModel* const tsm = browser()->GetTabStripModel();
  TabGroupId local_id = tsm->AddToNewGroup({0});

  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  group.SetLocalGroupId(local_id);
  GetSyncService()->AddGroup(group);

  STGTabsMenuModel model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  // Verify "Focus group" appears immediately after "Move group to new window".
  EXPECT_EQ(model.GetLabelAt(1),
            l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW));
  EXPECT_EQ(model.GetLabelAt(2),
            l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_FOCUS_GROUP));

  // Execute "Focus group".
  model.ActivatedAt(2);
  ASSERT_TRUE(tsm->GetFocusedGroup().has_value());
  EXPECT_EQ(tsm->GetFocusedGroup().value(), local_id);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Focus.EntryPoint",
      TabGroupFocusEntryPoint::kBookmarksBarContextMenu, 1);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.ContextMenu.BookmarksBar.Action",
      TabGroupMenuAction::Type::FOCUS_OR_UNFOCUS_GROUP, 1);

  // Rebuild model when group is focused; verify label updates to "Unfocus
  // group".
  STGTabsMenuModel focused_model(
      browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
  focused_model.Build(
      group, base::BindRepeating(
                 &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                 base::Unretained(this)));
  EXPECT_EQ(
      focused_model.GetLabelAt(2),
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNFOCUS_GROUP));

  // Execute "Unfocus group".
  focused_model.ActivatedAt(2);
  EXPECT_FALSE(tsm->GetFocusedGroup().has_value());
  histogram_tester.ExpectUniqueSample(
      "TabGroups.Focus.ExitReason",
      TabGroupFocusExitReason::kBookmarksBarContextMenu, 1);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.ContextMenu.BookmarksBar.Action",
      TabGroupMenuAction::Type::FOCUS_OR_UNFOCUS_GROUP, 2);
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelFocusingBrowserTest,
                       ToggleFocusGroupFromAppMenu) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  TabStripModel* const tsm = browser()->GetTabStripModel();
  TabGroupId local_id = tsm->AddToNewGroup({0});

  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {},
                      std::nullopt);
  group.SetLocalGroupId(local_id);
  GetSyncService()->AddGroup(group);

  STGTabsMenuModel model(browser(), TabGroupMenuContext::APP_MENU);
  model.Build(group,
              base::BindRepeating(
                  &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                  base::Unretained(this)));

  model.ActivatedAt(2);
  ASSERT_TRUE(tsm->GetFocusedGroup().has_value());
  EXPECT_EQ(tsm->GetFocusedGroup().value(), local_id);
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.EntryPoint",
                                      TabGroupFocusEntryPoint::kAppMenu, 1);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.ContextMenu.AppMenu.Action",
      TabGroupMenuAction::Type::FOCUS_OR_UNFOCUS_GROUP, 1);

  STGTabsMenuModel focused_model(browser(), TabGroupMenuContext::APP_MENU);
  focused_model.Build(
      group, base::BindRepeating(
                 &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
                 base::Unretained(this)));
  focused_model.ActivatedAt(2);
  EXPECT_FALSE(tsm->GetFocusedGroup().has_value());
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.ExitReason",
                                      TabGroupFocusExitReason::kAppMenu, 1);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.ContextMenu.AppMenu.Action",
      TabGroupMenuAction::Type::FOCUS_OR_UNFOCUS_GROUP, 2);
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupTabsMenuModelBrowserTest,
                       ContextMenuActionMetricsAcrossSources) {
  base::HistogramTester histogram_tester;
  if (DeletionDialogController::From(browser())) {
    DeletionDialogController::From(browser())
        ->SetPrefsPreventShowingDialogForTesting(
            /*should_prevent_dialog=*/true);
  }

  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  SavedTabGroupTab tab(GURL("about:blank"), u"Blank", group_guid,
                       /*position=*/0);
  SavedTabGroup group(u"Test Group", tab_groups::TabGroupColorId::kGrey, {tab},
                      /*position=*/std::nullopt, group_guid);
  GetSyncService()->AddGroup(group);

  // 1. Organizer Panel (Spice Rack): Open URL and Open in Browser.
  {
    STGTabsMenuModel organizer_model(browser(),
                                     TabGroupMenuContext::ORGANIZER_PANEL);
    organizer_model.Build(
        *GetSyncService()->GetGroup(group_guid),
        base::BindRepeating(
            &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
            base::Unretained(this)));

    std::optional<size_t> tab_index =
        GetIndexOfElementId(&organizer_model, STGTabsMenuModel::kTab);
    ASSERT_TRUE(tab_index.has_value());
    organizer_model.ActivatedAt(tab_index.value());
    histogram_tester.ExpectBucketCount(
        "TabGroups.ContextMenu.OrganizerPanel.Action",
        TabGroupMenuAction::Type::OPEN_URL, 1);

    std::optional<size_t> open_index =
        GetIndexOfElementId(&organizer_model, STGTabsMenuModel::kOpenGroup);
    ASSERT_TRUE(open_index.has_value());
    organizer_model.ActivatedAt(open_index.value());
    histogram_tester.ExpectBucketCount(
        "TabGroups.ContextMenu.OrganizerPanel.Action",
        TabGroupMenuAction::Type::OPEN_IN_BROWSER, 1);
  }

  // 2. Everything Menu: Pin/Unpin Group.
  {
    STGTabsMenuModel everything_model(
        browser(), TabGroupMenuContext::SAVED_TAB_GROUP_EVERYTHING_MENU);
    everything_model.Build(
        *GetSyncService()->GetGroup(group_guid),
        base::BindRepeating(
            &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
            base::Unretained(this)));

    std::optional<size_t> pin_index = GetIndexOfElementId(
        &everything_model, STGTabsMenuModel::kToggleGroupPinStateMenuItem);
    ASSERT_TRUE(pin_index.has_value());
    everything_model.ActivatedAt(pin_index.value());
    histogram_tester.ExpectUniqueSample(
        "TabGroups.ContextMenu.EverythingMenu.Action",
        TabGroupMenuAction::Type::PIN_OR_UNPIN_GROUP, 1);
  }

  // 3. Mac System Menu: Pin/Unpin Group.
  {
    STGTabsMenuModel mac_model(browser(), TabGroupMenuContext::MAC_SYSTEM_MENU);
    mac_model.Build(
        *GetSyncService()->GetGroup(group_guid),
        base::BindRepeating(
            &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
            base::Unretained(this)));

    std::optional<size_t> pin_index = GetIndexOfElementId(
        &mac_model, STGTabsMenuModel::kToggleGroupPinStateMenuItem);
    ASSERT_TRUE(pin_index.has_value());
    mac_model.ActivatedAt(pin_index.value());
    histogram_tester.ExpectUniqueSample(
        "TabGroups.ContextMenu.MacSystemMenu.Action",
        TabGroupMenuAction::Type::PIN_OR_UNPIN_GROUP, 1);
  }

  // 4. Bookmarks Bar Context Menu: Delete Group.
  {
    STGTabsMenuModel button_model(
        browser(), TabGroupMenuContext::SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU);
    button_model.Build(
        *GetSyncService()->GetGroup(group_guid),
        base::BindRepeating(
            &SavedTabGroupTabsMenuModelBrowserTest::GetNextCommandId,
            base::Unretained(this)));

    std::optional<size_t> delete_index = GetIndexOfElementId(
        &button_model, STGTabsMenuModel::kDeleteGroupMenuItem);
    ASSERT_TRUE(delete_index.has_value());
    button_model.ActivatedAt(delete_index.value());
    histogram_tester.ExpectUniqueSample(
        "TabGroups.ContextMenu.BookmarksBar.Action",
        TabGroupMenuAction::Type::DELETE_GROUP, 1);
  }
}

}  // namespace tab_groups
