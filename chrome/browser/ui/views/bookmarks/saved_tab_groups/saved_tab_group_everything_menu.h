// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_EVERYTHING_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_EVERYTHING_MENU_H_

#include "base/uuid.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace tab_groups {

class STGTabsMenuModel;
class TabGroupSyncService;

// A menu that contains a "Create new tab group" item and all the saved tab
// groups (if there are any) with color icon and tab group name. If no name is
// given, displays the number of tabs as menu label, e.g. "2 tabs".
class STGEverythingMenu : public views::MenuDelegate,
                          public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCreateNewTabGroup);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabGroup);

  STGEverythingMenu(views::MenuButtonController* menu_button_controller,
                    Browser* browser);

  STGEverythingMenu(const STGEverythingMenu&) = delete;
  STGEverythingMenu& operator=(const STGEverythingMenu&) = delete;
  ~STGEverythingMenu() override;

  // Popuates the Everything menu. If Everything menu is created via pressing on
  // Everything button `parent` will be the root MenuItemView. Otherwise if
  // Everything menu is constructed by hovering "Tab groups" option under 3-dot
  // menu, `parent` will be the MenuItemView that reprensents "Tab groups".
  void PopulateMenu(views::MenuItemView* parent);

  // Called at runtime when users hover a saved tab group to show the submenu.
  void PopulateTabGroupSubMenu(views::MenuItemView* parent);

  // App menu will be the delegate that gets queried if a submenu item should
  // be enabled. The app menu then asks `this` to confirm.
  bool ShouldEnableCommand(int command_id);

  // Runs the menu. Only called via pressing the Everything button.
  void RunMenu();

  bool IsShowing() { return menu_runner_ && menu_runner_->IsRunning(); }

  void SetShowSubmenu(bool show_submenu) { show_submenu_ = show_submenu; }

  // override views::MenuDelegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool ShowContextMenu(views::MenuItemView* source,
                       int command_id,
                       const gfx::Point& p,
                       ui::mojom::MenuSourceType source_type) override;
  bool GetAccelerator(int id, ui::Accelerator* accelerator) const override;

 private:
  class AppMenuSubMenuModelDelegate;

  int GenerateTabGroupCommandID(int idx_in_sorted_tab_groups);
  base::Uuid GetTabGroupIdFromCommandId(int command_id);
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel(
      TabGroupSyncService* tab_group_service);

  // Returns sorted saved tab groups with the most recently created as the
  // first, filtering out empty groups.
  std::vector<base::Uuid> GetGroupsForDisplaySortedByCreationTime(
      TabGroupSyncService* wrapper_service);

  // Because all the menu items (i.e. tab group items in the Everything menu -
  // primary menu and their submenus - secondary menu) need to be recognized and
  // invoked from the 3-dot menu so they follow the same pattern to get their
  // command ids, i.e. starts from a min command id
  // (AppMenuModel::kMinTabGroupsCommandId) and increments by a certain gap
  // (AppMenuModel::kNumUnboundedMenuTypes). When a command is invoked under the
  // app menu, AppMenu::IsTabGroupsCommand(id) recognizes it as a tab group
  // command then delegates to `this` (serves as the real delegate) to execute.
  // This function updates the latest command id which will be assigned to those
  // menu items. Here is an example: Suppose the starting command id is 100 and
  // it increments by 4. If we have 3 tab groups in primary menu and each has
  // only one tab, then if you open the submenu for the first tab group, the
  // command ids for all of them are:
  //
  // TabGroup1 - 100
  //        Open Group - 112
  //        Open/Move to New Window - 116
  //        Pin/Unpin Group - 120
  //        Delete Group - 124
  //        Tab - 128
  // TabGroup2 - 104
  // TabGroup3 - 108
  //
  // The command ids are assigned to primary menu items first, because the
  // secondary menu items are generated at run time only if you invoke the
  // submenu. That's why you have 112 as the first action's id.
  int GetAndIncrementLatestCommandId();

  // Saved tab groups with the most recently created as the first, and filtered
  // by their empty status.
  std::vector<base::Uuid> sorted_non_empty_tab_groups_;

  // Owned by the Everything button.
  raw_ptr<views::MenuButtonController> menu_button_controller_;

  // Whether or not a saved tab group item in the Everything menu should have
  // submenu. True for 3-dot menu.
  bool show_submenu_ = false;

  // The command id that gets updated and assigned to tab groups and their
  // submenu items.
  int latest_tab_group_command_id_ = -1;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<ui::SimpleMenuModel> groups_model_;

  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::map<base::Uuid, std::unique_ptr<STGTabsMenuModel>> tabs_models_;
  std::unique_ptr<AppMenuSubMenuModelDelegate> submenu_delegate_;
  std::optional<base::Uuid> latest_group_id_;

  raw_ptr<Browser> browser_;
  raw_ptr<views::Widget> widget_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_EVERYTHING_MENU_H_
