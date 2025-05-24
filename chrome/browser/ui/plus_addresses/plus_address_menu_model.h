// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_MENU_MODEL_H_
#define CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_MENU_MODEL_H_

#include <string>

#include "base/functional/callback.h"
#include "ui/menus/simple_menu_model.h"

namespace plus_addresses {

// A menu model for showing options to undo automatic plus address replacement
// during full form filling and managing plus addresses.
class PlusAddressMenuModel : public ui::SimpleMenuModel,
                             public ui::SimpleMenuModel::Delegate {
 public:
  // Command ids for the menu items.
  static constexpr int kUndoReplacement = 1;
  static constexpr int kManage = 2;

  // Creates a `PlusAddressMenuModel`. `undo_replacement` is the callback that
  // is expected to undo the filling of a plus address. `open_management` is
  // expected to open the management surface for plus addresses.
  PlusAddressMenuModel(const std::u16string& gaia_email,
                       base::OnceClosure undo_replacement,
                       base::RepeatingClosure open_management);
  ~PlusAddressMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  base::OnceClosure undo_replacement_;
  base::RepeatingClosure open_management_;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_MENU_MODEL_H_
