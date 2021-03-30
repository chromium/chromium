// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/move_to_desks_menu_model.h"

#include "ash/public/cpp/desks_helper.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

MoveToDesksMenuModel::MoveToDesksMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    views::Widget* browser_widget)
    : SimpleMenuModel(delegate),
      desks_helper_(ash::DesksHelper::Get()),
      browser_widget_(browser_widget) {
  constexpr int kMaxNumberOfDesks = IDC_MOVE_TO_DESK_8 - IDC_MOVE_TO_DESK_1 + 1;
  AddCheckItem(IDC_TOGGLE_ASSIGN_TO_ALL_DESKS,
               l10n_util::GetStringUTF16(IDS_ASSIGN_TO_ALL_DESKS));
  assign_to_all_desks_item_index_ = GetItemCount() - 1;
  AddSeparator(ui::NORMAL_SEPARATOR);

  // Store the number of items that occur before the group of move to desk items
  // so we can retrieve the move to desk items' indices properly.
  move_to_desk_group_offset_ = GetItemCount();
  for (int i = 0; i < kMaxNumberOfDesks; ++i)
    AddCheckItem(IDC_MOVE_TO_DESK_1 + i, std::u16string());
}

bool MoveToDesksMenuModel::MayHaveMnemonicsAt(int index) const {
  // If the label is a user-created desk name, the user might have ampersands so
  // don't process mnemonics for them.
  return (index == assign_to_all_desks_item_index_);
}

bool MoveToDesksMenuModel::IsVisibleAt(int index) const {
  if (index == assign_to_all_desks_item_index_)
    return true;
  return OffsetIndexForMoveToDeskGroup(index) <
         desks_helper_->GetNumberOfDesks();
}

std::u16string MoveToDesksMenuModel::GetLabelAt(int index) const {
  if (index == assign_to_all_desks_item_index_)
    return l10n_util::GetStringUTF16(IDS_ASSIGN_TO_ALL_DESKS);
  return desks_helper_->GetDeskName(OffsetIndexForMoveToDeskGroup(index));
}

bool MoveToDesksMenuModel::IsItemCheckedAt(int index) const {
  const bool is_assigned_to_all_desks =
      browser_widget_ && browser_widget_->IsVisibleOnAllWorkspaces();

  if (index == assign_to_all_desks_item_index_)
    return is_assigned_to_all_desks;

  return !is_assigned_to_all_desks && OffsetIndexForMoveToDeskGroup(index) ==
                                          desks_helper_->GetActiveDeskIndex();
}

int MoveToDesksMenuModel::OffsetIndexForMoveToDeskGroup(int index) const {
  const int move_to_desk_group_index = index - move_to_desk_group_offset_;
  DCHECK_GE(move_to_desk_group_index, 0);
  return move_to_desk_group_index;
}
