// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MOVE_TO_DESKS_MENU_MODEL_H_
#define CHROMEOS_UI_FRAME_MOVE_TO_DESKS_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

namespace chromeos {

// A menu model that builds the contents of the Move to Desks menu.
class MoveToDesksMenuModel : public ui::SimpleMenuModel {
 public:
  // The command id for showing the Move to Desks menu. This is an arbitrary
  // number that must not collide with other command ids. If this needs to be
  // updated, choose an unused number.
  static constexpr int kMenuCommandId = 40985;

  // If `add_title` is true, a title will be added to the Move to Desks menu.
  // Should be set to true if this is a standalone menu and not a submenu.
  explicit MoveToDesksMenuModel(
      std::unique_ptr<ui::SimpleMenuModel::Delegate> delegate,
      bool add_title = false);
  MoveToDesksMenuModel(const MoveToDesksMenuModel&) = delete;
  MoveToDesksMenuModel& operator=(const MoveToDesksMenuModel&) = delete;
  ~MoveToDesksMenuModel() override;

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
  // A menu delegate used to determine which labels are shown and enabled. Also
  // handles how different command ids are handled.
  std::unique_ptr<ui::SimpleMenuModel::Delegate> delegate_;

  // This is the index of the assign to all desks item in the menu model.
  int assign_to_all_desks_item_index_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MOVE_TO_DESKS_MENU_MODEL_H_
