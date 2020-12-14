// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/assign_to_desks_menu_model.h"

#include "ash/public/cpp/desks_helper.h"
#include "chrome/app/chrome_command_ids.h"

AssignToDesksMenuModel::AssignToDesksMenuModel(
    ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate), desks_helper_(ash::DesksHelper::Get()) {
  constexpr int kMaxNumberOfDesks = IDC_SEND_TO_DESK_8 - IDC_SEND_TO_DESK_1 + 1;
  for (int i = 0; i < kMaxNumberOfDesks; ++i)
    AddCheckItem(IDC_SEND_TO_DESK_1 + i, base::string16());
}

bool AssignToDesksMenuModel::IsVisibleAt(int index) const {
  return index < desks_helper_->GetNumberOfDesks();
}

base::string16 AssignToDesksMenuModel::GetLabelAt(int index) const {
  return desks_helper_->GetDeskName(index);
}

bool AssignToDesksMenuModel::IsItemCheckedAt(int index) const {
  return index == desks_helper_->GetActiveDeskIndex();
}
