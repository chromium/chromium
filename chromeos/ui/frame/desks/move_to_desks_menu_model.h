// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_DESKS_MOVE_TO_DESKS_MENU_MODEL_H_
#define CHROMEOS_UI_FRAME_DESKS_MOVE_TO_DESKS_MENU_MODEL_H_

#include "ui/base/models/simple_menu_model.h"

namespace chromeos {

// A menu model that builds the contents of the Move to Desks menu.
class MoveToDesksMenuModel : public ui::SimpleMenuModel {
 public:
  // The command id for showing the Move to Desks menu. This is an arbitrary
  // number that must not collide with other command ids. If this needs to be
  // updated, choose an unused number.
  static constexpr int kMenuCommandId = 40800;

  // If `add_title` is true, a title will be added to the Move to Desks menu.
  // Should be set to true if this is a standalone menu and not a submenu.
  explicit MoveToDesksMenuModel(
      std::unique_ptr<ui::SimpleMenuModel::Delegate> delegate,
      bool add_title = false);
  MoveToDesksMenuModel(const MoveToDesksMenuModel&) = delete;
  MoveToDesksMenuModel& operator=(const MoveToDesksMenuModel&) = delete;
  ~MoveToDesksMenuModel() override;

  // To avoid colliding with other command ids, start the sequence of command
  // ids from |kMenuCommandId| + 1. Also give them explicit values to make the
  // command ids more discoverable. See crbug.com/1222475.
  enum CommandId {
    MOVE_TO_DESK_1 = 40801,
    MOVE_TO_DESK_2 = 40802,
    MOVE_TO_DESK_3 = 40803,
    MOVE_TO_DESK_4 = 40804,
    MOVE_TO_DESK_5 = 40805,
    MOVE_TO_DESK_6 = 40806,
    MOVE_TO_DESK_7 = 40807,
    MOVE_TO_DESK_8 = 40808,
    MOVE_TO_DESK_9 = 40809,
    MOVE_TO_DESK_10 = 40810,
    MOVE_TO_DESK_11 = 40811,
    MOVE_TO_DESK_12 = 40812,
    MOVE_TO_DESK_13 = 40813,
    MOVE_TO_DESK_14 = 40814,
    MOVE_TO_DESK_15 = 40815,
    MOVE_TO_DESK_16 = 40816,
    TOGGLE_ASSIGN_TO_ALL_DESKS = 40817,
  };

  // SimpleMenuModel:
  bool MayHaveMnemonicsAt(size_t index) const override;

 private:
  // A menu delegate used to determine which labels are shown and enabled. Also
  // handles how different command ids are handled.
  std::unique_ptr<ui::SimpleMenuModel::Delegate> delegate_;

  // This is the index of the assign to all desks item in the menu model.
  // Floated windows cannot be assigned to all desks. In that case, this will be
  // nullopt.
  std::optional<size_t> assign_to_all_desks_item_index_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_DESKS_MOVE_TO_DESKS_MENU_MODEL_H_
