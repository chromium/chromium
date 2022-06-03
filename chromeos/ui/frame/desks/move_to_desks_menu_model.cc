// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"

#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

MoveToDesksMenuModel::MoveToDesksMenuModel(
    std::unique_ptr<ui::SimpleMenuModel::Delegate> delegate,
    bool add_title)
    : SimpleMenuModel(delegate.get()), delegate_(std::move(delegate)) {
  static_assert(MOVE_TO_DESK_1 == MOVE_TO_DESK_2 - 1 &&
                    MOVE_TO_DESK_2 == MOVE_TO_DESK_3 - 1 &&
                    MOVE_TO_DESK_3 == MOVE_TO_DESK_4 - 1 &&
                    MOVE_TO_DESK_4 == MOVE_TO_DESK_5 - 1 &&
                    MOVE_TO_DESK_5 == MOVE_TO_DESK_6 - 1 &&
                    MOVE_TO_DESK_6 == MOVE_TO_DESK_7 - 1 &&
                    MOVE_TO_DESK_7 == MOVE_TO_DESK_8 - 1,
                "MOVE_TO_DESK_* commands must be in order.");

  if (add_title)
    AddTitle(l10n_util::GetStringUTF16(IDS_MOVE_TO_DESKS_MENU));

  AddCheckItem(TOGGLE_ASSIGN_TO_ALL_DESKS,
               l10n_util::GetStringUTF16(IDS_ASSIGN_TO_ALL_DESKS));
  assign_to_all_desks_item_index_ = GetItemCount() - 1;
  AddSeparator(ui::NORMAL_SEPARATOR);

  constexpr int kMaxNumberOfDesks = MOVE_TO_DESK_8 - MOVE_TO_DESK_1 + 1;
  for (int i = 0; i < kMaxNumberOfDesks; ++i)
    AddCheckItem(MOVE_TO_DESK_1 + i, std::u16string());
}

MoveToDesksMenuModel::~MoveToDesksMenuModel() {}

bool MoveToDesksMenuModel::MayHaveMnemonicsAt(int index) const {
  // If the label is a user-created desk name, the user might have ampersands so
  // don't process mnemonics for them.
  return (index == assign_to_all_desks_item_index_);
}

}  // namespace chromeos
