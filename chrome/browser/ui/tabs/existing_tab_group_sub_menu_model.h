// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_TAB_GROUP_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_TAB_GROUP_SUB_MENU_MODEL_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"

class TabStripModel;
class TabMenuModelDelegate;

namespace tab_groups {
class TabGroupId;
}

class ExistingTabGroupSubMenuModel : public ExistingBaseSubMenuModel {
 public:
  ExistingTabGroupSubMenuModel(ui::SimpleMenuModel::Delegate* parent_delegate,
                               TabMenuModelDelegate* tab_menu_model_delegate,
                               TabStripModel* model,
                               int context_index);
  ExistingTabGroupSubMenuModel(const ExistingTabGroupSubMenuModel&) = delete;
  ExistingTabGroupSubMenuModel& operator=(const ExistingTabGroupSubMenuModel&) =
      delete;
  ~ExistingTabGroupSubMenuModel() override;

  // Whether the submenu should be shown in the provided context. True iff
  // the submenu would show at least one group. Does not assume ownership of
  // |model|; |model| must outlive this instance.
  static bool ShouldShowSubmenu(TabStripModel* model,
                                int context_index,
                                TabMenuModelDelegate* tab_menu_model_delegate);

  // Used for testing.
  void ExecuteExistingCommandForTesting(size_t target_index);
  size_t GetDisplayedGroupCount() {
    return target_index_to_group_mapping_.size();
  }

 private:
  // ExistingBaseSubMenuModel
  void ExecuteExistingCommand(size_t target_index) override;

  // Retrieves all tab groups ids from the given model.
  const std::vector<tab_groups::TabGroupId> GetGroupsFromModel(
      TabStripModel* current_model);

  // Retrieves all menu items from the given model.
  const std::vector<MenuItemInfo> GetMenuItemsFromModel(
      TabStripModel* current_model);

  // Returns a list of closed saved tab groups, provided that
  // |tab_menu_model_delegate| is valid and has access to an instance of
  // |TabGroupSyncService|.
  static std::vector<base::Uuid> GetClosedSavedTabGroups(
      TabMenuModelDelegate* tab_menu_model_delegate);

  // Helper function to make a |MenuItemInfo| corresponding to the fields
  // of a group. Intended to be used for both saved tab groups and local groups.
  MenuItemInfo CreateMenuItemInfo(const std::u16string& displayed_title,
                                  const tab_groups::TabGroupColorId&);

  // Creates a |MenuItemInfo| for each closed saved tab group and appends
  // it to the end of |existing_menu_item_infos|.
  void AppendMenuItemInfosFromSavedTabGroups(
      std::vector<MenuItemInfo>& existing_menu_item_infos);

  // Whether the submenu should contain the group |group|. True iff at least
  // one tab that would be affected by the command is not in |group|.
  static bool ShouldShowGroup(TabStripModel* model,
                              int context_index,
                              tab_groups::TabGroupId group);

  // Returns a container of selected tab indices for specifying which
  // tabs get added to a group. Returns {GetContentIndex()} if the
  // content index is not selected.
  std::vector<int> GetSelectedIndices();

  // Takes the tabs at indices specified by |GetSelectedIndices()| and
  // adds them to the saved tab group with the given id.
  void AddSelectedTabsToSavedGroup(const base::Uuid&);

  // Takes the tabs at indices specified by |GetSelectedIndices()| and
  // adds them to the open tab group with the given local tab group id.
  void AddSelectedTabsToOpenGroup(const tab_groups::TabGroupId&);

  // Mapping of the initial tab group to index in the menu model. this must
  // be used in cases where the tab groups returned from
  // GetOrderedTabGroupsInSubMenu changes after the menu has been opened but
  // before the action is taken from the menumodel.
  std::map<size_t, tab_groups::EitherGroupID> target_index_to_group_mapping_;

  // Used to retrieve a list of browsers which potentially hold tab groups.
  const raw_ptr<TabMenuModelDelegate> tab_menu_model_delegate_;
};

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_TAB_GROUP_SUB_MENU_MODEL_H_
