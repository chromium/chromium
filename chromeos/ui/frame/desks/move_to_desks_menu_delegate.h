// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_DESKS_MOVE_TO_DESKS_MENU_DELEGATE_H_
#define CHROMEOS_UI_FRAME_DESKS_MOVE_TO_DESKS_MENU_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace chromeos {

// A `ui::SimpleMenuModel::Delegate` for the Move to Desks menu.
class MoveToDesksMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MoveToDesksMenuDelegate(views::Widget* widget);
  MoveToDesksMenuDelegate(const MoveToDesksMenuDelegate&) = delete;
  MoveToDesksMenuDelegate& operator=(const MoveToDesksMenuDelegate&) = delete;
  ~MoveToDesksMenuDelegate() override = default;

  // Returns whether the move to desks menu should be shown, i.e. there are more
  // than two desks.
  static bool ShouldShowMoveToDesksMenu(aura::Window* window);

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // This is indirectly owned by BrowserFrame, and guaranteed to be destroyed
  // before Widget.
  const raw_ptr<views::Widget> widget_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_DESKS_MOVE_TO_DESKS_MENU_DELEGATE_H_
