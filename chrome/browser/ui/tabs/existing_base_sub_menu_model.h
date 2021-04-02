// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_EXISTING_BASE_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_EXISTING_BASE_SUB_MENU_MODEL_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/optional.h"
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
                                 ui::SimpleMenuModel::Delegate {
 public:
  ExistingBaseSubMenuModel(ui::SimpleMenuModel::Delegate* parent_delegate,
                           TabStripModel* model,
                           int context_index,
                           int min_command_id);

  // ui::SimpleMenuModel
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // ui::SimpleMenuModel::Delegate
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) final;

  // Command IDs for various submenus.
  static constexpr int kMinExistingWindowCommandId = 1001;
  static constexpr int kMinExistingTabGroupCommandId = 1301;

  ~ExistingBaseSubMenuModel() override;

 protected:
  struct MenuItemInfo {
    explicit MenuItemInfo(const std::u16string menu_text);
    MenuItemInfo(const std::u16string& menu_text, ui::ImageModel menu_image);
    MenuItemInfo(const MenuItemInfo& menu_item_info);
    ~MenuItemInfo();

    // The text for an entry in the sub menu.
    const std::u16string text;

    // The optional image for an entry in the sub menu.
    base::Optional<ui::ImageModel> image;

    bool may_have_mnemonics = true;
  };

  // Helper method to create consistent submenus.|new_text| is the label to add
  // the tab to a new object model (e.g. group or window). |menu_item_infos| is
  // a vector of text and optionally images for adding the tab to an existing
  // object model.
  void Build(int new_text, std::vector<MenuItemInfo> menu_item_infos);

  // Helper method for checking if the command is to add a tab to a new object
  // model.
  bool IsNewCommand(int command_id) const {
    return command_id == min_command_id_;
  }

  // Performs the action for adding the tab to a new object model (e.g. group or
  // window).
  virtual void ExecuteNewCommand(int event_flags);

  // Performs the action for adding the tab to an existing object model (e.g.
  // group or window).
  virtual void ExecuteExistingCommand(int command_index);

  // Maximum number of entries for a submenu.
  static constexpr int max_size = 200;

  ui::SimpleMenuModel::Delegate* parent_delegate() const {
    return parent_delegate_;
  }
  TabStripModel* model() { return model_; }
  int GetContextIndex() const;

 private:
  ui::SimpleMenuModel::Delegate* parent_delegate_;
  TabStripModel* model_;
  content::WebContents* context_contents_;
  int min_command_id_;
  DISALLOW_COPY_AND_ASSIGN(ExistingBaseSubMenuModel);
};

#endif  // CHROME_BROWSER_UI_TABS_EXISTING_BASE_SUB_MENU_MODEL_H_
