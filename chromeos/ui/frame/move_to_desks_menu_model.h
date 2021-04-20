// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MOVE_TO_DESKS_MENU_MODEL_H_
#define CHROMEOS_UI_FRAME_MOVE_TO_DESKS_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

namespace chromeos {

// A menu model that builds the contents of the move to desks menu.
class MoveToDesksMenuModel : public ui::SimpleMenuModel {
 public:
  MoveToDesksMenuModel(std::unique_ptr<ui::SimpleMenuModel::Delegate> delegate,
                       bool add_title = false);
  MoveToDesksMenuModel(const MoveToDesksMenuModel&) = delete;
  MoveToDesksMenuModel& operator=(const MoveToDesksMenuModel&) = delete;
  ~MoveToDesksMenuModel() override;

  static constexpr int kMenuCommandId = 40985;

  enum CommandId {
    MOVE_TO_DESK_1 = 1,
    MOVE_TO_DESK_2 = 2,
    MOVE_TO_DESK_3 = 3,
    MOVE_TO_DESK_4 = 4,
    MOVE_TO_DESK_5 = 5,
    MOVE_TO_DESK_6 = 6,
    MOVE_TO_DESK_7 = 7,
    MOVE_TO_DESK_8 = 8,
    TOGGLE_ASSIGN_TO_ALL_DESKS = 9,
  };

  // SimpleMenuModel:
  bool MayHaveMnemonicsAt(int index) const override;

 private:
  // |this| owns its |delegate|.
  std::unique_ptr<ui::SimpleMenuModel::Delegate> delegate_;

  // This is the index of the assign to all desks item in the menu model.
  int assign_to_all_desks_item_index_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MOVE_TO_DESKS_MENU_MODEL_H_
