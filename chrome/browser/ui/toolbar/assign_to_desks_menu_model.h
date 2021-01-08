// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_ASSIGN_TO_DESKS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_ASSIGN_TO_DESKS_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

namespace ash {
class DesksHelper;
}

namespace views {
class Widget;
}

// A menu model that builds the contents of the assign to desks menu.
class AssignToDesksMenuModel : public ui::SimpleMenuModel {
 public:
  AssignToDesksMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                         views::Widget* browser_widget);
  ~AssignToDesksMenuModel() override = default;

  // SimpleMenuModel:
  bool IsVisibleAt(int index) const override;
  base::string16 GetLabelAt(int index) const override;
  bool IsItemCheckedAt(int index) const override;

 private:
  // Returns |index| - |send_to_desk_group_offset_|. This value is the relative
  // index within the group of send to desk items. This value must be greater
  // than or equal to 0.
  int OffsetIndexForSendToDeskGroup(int index) const;

  const ash::DesksHelper* const desks_helper_;
  const views::Widget* const browser_widget_;

  // This is the index of the assign to all desks item in the menu model.
  int assign_to_all_desks_item_index_;

  // This is the number of items in the menu model that occurs before the group
  // of send to desk items.
  int send_to_desk_group_offset_;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_ASSIGN_TO_DESKS_MENU_MODEL_H_
