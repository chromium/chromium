// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_EVERYTHING_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_EVERYTHING_MENU_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/dialog_model_context_menu_controller.h"

namespace tab_groups {

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

  // Runs the menu. Only called via pressing the Everything button.
  void RunMenu();

  bool IsShowing() { return menu_runner_ && menu_runner_->IsRunning(); }

  void SetShowPinOption(bool show_pin_unpin_option) {
    show_pin_unpin_option_ = show_pin_unpin_option;
  }

  // override views::MenuDelegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool ShowContextMenu(views::MenuItemView* source,
                       int command_id,
                       const gfx::Point& p,
                       ui::MenuSourceType source_type) override;

 private:
  friend class STGEverythingMenuUnitTest;

  int GetAndIncrementNextTabGroupItemID();
  const SavedTabGroup* GetTabGroupForCommandId(int command_id);
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel();

  // Returns sorted saved tab groups with the most recently created as the
  // first.
  std::vector<const SavedTabGroup*> GetSortedTabGroupsByCreationTime(
      const SavedTabGroupModel* stg_model);

  const SavedTabGroupModel* GetSavedTabGroupModelFromBrowser();

  // Saved tab groups with the most recently created as the first.
  std::vector<const SavedTabGroup*> sorted_tab_groups_;

  // Owned by the Everything button.
  raw_ptr<views::MenuButtonController> menu_button_controller_;

  // Whether or not the submenu of a tab group item should have "Pin/Unpin group
  // from bookmarks bar" option. True by default. False if the submenu if
  // visually far away from the bookmark bar e.g. in 3-dot menu.
  bool show_pin_unpin_option_ = true;

  // The convenient controller that runs a context menu for saved tab group menu
  // items.
  std::unique_ptr<views::DialogModelContextMenuController>
      context_menu_controller_;

  // TODO(pengchaocai): Explore a generic command id solution if there are more
  // use cases than app menu.
  int next_tab_group_item_command_id_ = AppMenuModel::kMinTabGroupsCommandId;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<ui::SimpleMenuModel> model_;
  raw_ptr<Browser> browser_;
  raw_ptr<views::Widget> widget_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_EVERYTHING_MENU_H_
