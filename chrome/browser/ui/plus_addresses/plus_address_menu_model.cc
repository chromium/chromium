// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_menu_model.h"

#include <string>
#include <utility>

#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {

PlusAddressMenuModel::PlusAddressMenuModel(
    const std::u16string& gaia_email,
    base::OnceClosure undo_replacement,
    base::RepeatingClosure open_management)
    : ui::SimpleMenuModel(/*delegate=*/this),
      undo_replacement_(std::move(undo_replacement)),
      open_management_(std::move(open_management)) {
  AddItem(kUndoReplacement,
          l10n_util::GetStringFUTF16(IDS_PLUS_ADDRESS_FULL_FORM_FILL_TOAST_UNDO,
                                     gaia_email));
  AddItem(kManage, l10n_util::GetStringUTF16(
                       IDS_PLUS_ADDRESS_FULL_FORM_FILL_TOAST_MANAGE));
}

PlusAddressMenuModel::~PlusAddressMenuModel() = default;

void PlusAddressMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case kUndoReplacement:
      // Safeguard against the menu item getting run multiple times.
      if (undo_replacement_) {
        std::exchange(undo_replacement_, base::OnceClosure()).Run();
      }
      break;
    case kManage:
      if (open_management_) {
        open_management_.Run();
      }
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace plus_addresses
