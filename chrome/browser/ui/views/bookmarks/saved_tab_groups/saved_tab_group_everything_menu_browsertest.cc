// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace tab_groups {

namespace {
SavedTabGroup CreateTestSavedTabGroup() {
  base::Uuid id = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Test Test";
  const tab_groups::TabGroupColorId& color = tab_groups::TabGroupColorId::kBlue;

  SavedTabGroupTab tab1(GURL("www.google.com"), u"Google", id, /*position=*/0);
  SavedTabGroupTab tab2(GURL("chrome://newtab"), u"new tab", id,
                        /*position=*/1);

  std::vector<SavedTabGroupTab> tabs = {tab1, tab2};
  SavedTabGroup group(title, color, tabs, /*position=*/std::nullopt, id);
  return group;
}
}  // namespace

class STGEverythingMenuBrowserTest : public InProcessBrowserTest {
 public:
  TabGroupSyncService* service() {
    return TabGroupSyncServiceFactory::GetForProfile(browser()->GetProfile());
  }
};

IN_PROC_BROWSER_TEST_F(STGEverythingMenuBrowserTest,
                       ExecuteCommandOutOfBoundsCrash) {
  // Add one tab group.
  SavedTabGroup group = CreateTestSavedTabGroup();
  service()->AddGroup(group);

  auto everything_menu = std::make_unique<STGEverythingMenu>(
      nullptr, browser(), STGEverythingMenu::MenuContext::kSavedTabGroupBar);

  auto parent = std::make_unique<views::MenuItemView>();
  // This will populate sorted_non_empty_tab_groups_ with 1 group.
  everything_menu->PopulateMenu(parent.get());

  // kMinTabGroupsCommandId is for the first tab group.
  // kMinTabGroupsCommandId + kGap would be for a second tab group (which
  // doesn't exist).
  int invalid_command_id = AppMenuModel::kMinTabGroupsCommandId +
                           AppMenuModel::kNumUnboundedMenuTypes;

  // This should NOT crash.
  everything_menu->ExecuteCommand(invalid_command_id, 0);

  everything_menu.reset();
  parent.reset();
}

IN_PROC_BROWSER_TEST_F(STGEverythingMenuBrowserTest, ExecuteCommandValid) {
  // Add one tab group.
  SavedTabGroup group = CreateTestSavedTabGroup();
  service()->AddGroup(group);

  auto everything_menu = std::make_unique<STGEverythingMenu>(
      nullptr, browser(), STGEverythingMenu::MenuContext::kSavedTabGroupBar);

  auto parent = std::make_unique<views::MenuItemView>();
  everything_menu->PopulateMenu(parent.get());

  // kMinTabGroupsCommandId is for the first tab group.
  int valid_command_id = AppMenuModel::kMinTabGroupsCommandId;

  // This should NOT crash and should execute the command.
  everything_menu->ExecuteCommand(valid_command_id, 0);

  everything_menu.reset();
  parent.reset();
}

IN_PROC_BROWSER_TEST_F(STGEverythingMenuBrowserTest,
                       AppMenuSubmenuAccessedMetrics) {
  base::HistogramTester histogram_tester;
  SavedTabGroup group = CreateTestSavedTabGroup();
  service()->AddGroup(group);

  auto app_menu_stg = std::make_unique<STGEverythingMenu>(
      nullptr, browser(), STGEverythingMenu::MenuContext::kAppMenu);

  auto parent = std::make_unique<views::MenuItemView>();
  app_menu_stg->PopulateMenu(parent.get());

  // Populating the parent menu should not record an access.
  histogram_tester.ExpectTotalCount("TabGroups.ContextMenu.Accessed", 0);

  views::MenuItemView* group_item =
      parent->GetMenuItemByID(AppMenuModel::kMinTabGroupsCommandId);
  ASSERT_TRUE(group_item);

  // Showing the group submenu records an access each time it is shown.
  app_menu_stg->WillShowMenu(group_item);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.ContextMenu.Accessed",
      saved_tab_groups::metrics::SavedTabGroupOpenedSubmenuDesktop::kAppMenu,
      1);

  app_menu_stg->WillShowMenu(group_item);
  histogram_tester.ExpectUniqueSample(
      "TabGroups.ContextMenu.Accessed",
      saved_tab_groups::metrics::SavedTabGroupOpenedSubmenuDesktop::kAppMenu,
      2);

  app_menu_stg.reset();
  parent.reset();
}

IN_PROC_BROWSER_TEST_F(STGEverythingMenuBrowserTest,
                       EverythingMenuContextMenuAccessedMetrics) {
  base::HistogramTester histogram_tester;
  SavedTabGroup group = CreateTestSavedTabGroup();
  service()->AddGroup(group);

  auto everything_menu = std::make_unique<STGEverythingMenu>(
      nullptr, browser(), STGEverythingMenu::MenuContext::kSavedTabGroupBar);

  auto parent = std::make_unique<views::MenuItemView>();
  everything_menu->PopulateMenu(parent.get());

  histogram_tester.ExpectTotalCount("TabGroups.ContextMenu.Accessed", 0);

  // Right-clicking "Create new tab group" does not open a group context menu.
  EXPECT_FALSE(everything_menu->ShowContextMenu(
      parent.get(), IDC_CREATE_NEW_TAB_GROUP, gfx::Point(),
      ui::mojom::MenuSourceType::kMouse));
  histogram_tester.ExpectTotalCount("TabGroups.ContextMenu.Accessed", 0);

  // Right-clicking a saved tab group item opens its context menu and logs the
  // metric.
  EXPECT_TRUE(everything_menu->ShowContextMenu(
      parent.get(), AppMenuModel::kMinTabGroupsCommandId, gfx::Point(),
      ui::mojom::MenuSourceType::kMouse));
  histogram_tester.ExpectUniqueSample(
      "TabGroups.ContextMenu.Accessed",
      saved_tab_groups::metrics::SavedTabGroupOpenedSubmenuDesktop::
          kEverythingMenu,
      1);

  everything_menu.reset();
  parent.reset();
}

}  // namespace tab_groups
