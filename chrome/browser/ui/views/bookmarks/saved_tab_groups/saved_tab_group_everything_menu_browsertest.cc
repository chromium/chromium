// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"

#include <memory>

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"

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
    return TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
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

}  // namespace tab_groups
