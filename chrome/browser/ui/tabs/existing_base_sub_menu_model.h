// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_BASE_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_BASE_SUB_MENU_MODEL_H_

#include <stddef.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"

class TabStripModel;

namespace content {
class WebContents;
}

// Base class for creating submenus for the tab context menu. This enforces the
// format of the submenu as follows:
// - guaranteed unique IDs for different submenus
// - the visual layout of the submenu
//  - the first item of the submenu should be the option to add a tab to a new
//    object model (e.g. group or window)
//  - the next item in the menu should be a separator
//  - a maximum of 200 items to add a tab to an existing model
class ExistingBaseSubMenuModel : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  ExistingBaseSubMenuModel(ui::SimpleMenuModel::Delegate* parent_delegate,
                           TabStripModel* model,
                           int context_index,
                           int min_command_id,
                           int parent_new_command_id_);

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdAlerted(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) final;

  // Command IDs for various submenus.
  static constexpr int kMinExistingWindowCommandId = 1001;
  static constexpr int kMinExistingTabGroupCommandId = 1301;

  ExistingBaseSubMenuModel(const ExistingBaseSubMenuModel&) = delete;
  ExistingBaseSubMenuModel& operator=(const ExistingBaseSubMenuModel&) = delete;

  ~ExistingBaseSubMenuModel() override;

  const base::flat_map<int, size_t>& command_id_to_target_index_for_testing() {
    return command_id_to_target_index_;
  }

 protected:
  struct MenuItemInfo {
    explicit MenuItemInfo(const std::u16string menu_text);
    MenuItemInfo(const std::u16string& menu_text, ui::ImageModel menu_image);
    MenuItemInfo(const MenuItemInfo& menu_item_info);
    ~MenuItemInfo();

    // The text for an entry in the sub menu.
    std::u16string text;

    // The optional image for an entry in the sub menu.
    std::optional<ui::ImageModel> image;

    bool may_have_mnemonics = true;

    // The target index for this item. E.g. tab group index or browser
    // index. If this field is not provided then the entry for this item will be
    // a title and have no corresponding command.
    std::optional<size_t> target_index;

    // An optionally provided accessible name for this menu item. If
    // |accessible_name| is empty, then the default accessible name will be used
    // for this item.
    std::u16string accessible_name;
  };

  // Helper method to create consistent submenus.|new_text| is the label to add
  // the tab to a new object model (e.g. group or window). |menu_item_infos| is
  // a vector of text and optionally images for adding the tab to an existing
  // object model.
  void Build(int new_text, std::vector<MenuItemInfo> menu_item_infos);

  // Clears the MenuModel and |command_id_to_target_index_|.
  void ClearMenu();

  // Helper method for checking if the command is to add a tab to a new object
  // model.
  bool IsNewCommand(int command_id) const;

  // Performs the action for adding the tab to an existing object model (e.g.
  // group or window) at |target_index|.
  virtual void ExecuteExistingCommand(size_t target_index);

  // Maximum number of entries for a submenu.
  static constexpr size_t max_size = 200;

  ui::SimpleMenuModel::Delegate* parent_delegate() const {
    return parent_delegate_;
  }
  TabStripModel* model() { return model_; }
  int GetContextIndex() const;

 private:
  const raw_ptr<ui::SimpleMenuModel::Delegate> parent_delegate_;
  const raw_ptr<TabStripModel, DanglingUntriaged> model_;
  const raw_ptr<const content::WebContents, DanglingUntriaged>
      context_contents_;
  const int min_command_id_;
  const int parent_new_command_id_;

  // Stores a mapping from a menu item's command id to its target index (e.g.
  // tab-group index or browser index).
  base::flat_map<int, size_t> command_id_to_target_index_;
};

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_BASE_SUB_MENU_MODEL_H_
