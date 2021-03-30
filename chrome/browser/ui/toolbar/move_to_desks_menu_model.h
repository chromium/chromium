// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MOVE_TO_DESKS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_MOVE_TO_DESKS_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

namespace ash {
class DesksHelper;
}

namespace views {
class Widget;
}

// A menu model that builds the contents of the move to desks menu.
class MoveToDesksMenuModel : public ui::SimpleMenuModel {
 public:
  MoveToDesksMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                       views::Widget* browser_widget);
  ~MoveToDesksMenuModel() override = default;

  // SimpleMenuModel:
  bool MayHaveMnemonicsAt(int index) const override;
  bool IsVisibleAt(int index) const override;
  std::u16string GetLabelAt(int index) const override;
  bool IsItemCheckedAt(int index) const override;

 private:
  // Returns |index| - |move_to_desk_group_offset_|. This value is the relative
  // index within the group of move to desk items. This value must be greater
  // than or equal to 0.
  int OffsetIndexForMoveToDeskGroup(int index) const;

  const ash::DesksHelper* const desks_helper_;
  const views::Widget* const browser_widget_;

  // This is the index of the assign to all desks item in the menu model.
  int assign_to_all_desks_item_index_;

  // This is the number of items in the menu model that occurs before the group
  // of move to desk items.
  int move_to_desk_group_offset_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MOVE_TO_DESKS_MENU_MODEL_H_
