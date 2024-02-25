// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/desks/move_to_desks_menu_delegate.h"

#include "base/metrics/histogram_macros.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/desks/move_to_desks_menu_model.h"
#include "chromeos/ui/wm/desks/chromeos_desks_histogram_enums.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace {

int MapCommandIdToDeskIndex(int command_id) {
  DCHECK_GE(command_id, chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_1);
  DCHECK_LE(command_id, chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_16);
  return command_id - chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_1;
}

bool IsAssignToAllDesksCommand(int command_id) {
  return command_id ==
         chromeos::MoveToDesksMenuModel::TOGGLE_ASSIGN_TO_ALL_DESKS;
}

bool IsMoveToDeskCommand(int command_id) {
  return command_id >= chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_1 &&
         command_id <= chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_16;
}

}  // namespace

namespace chromeos {

MoveToDesksMenuDelegate::MoveToDesksMenuDelegate(views::Widget* widget)
    : widget_(widget) {}

// static
bool MoveToDesksMenuDelegate::ShouldShowMoveToDesksMenu(aura::Window* window) {
  return DesksHelper::Get(window)->GetNumberOfDesks() > 1;
}

bool MoveToDesksMenuDelegate::IsCommandIdChecked(int command_id) const {
  const bool assigned_to_all_desks = widget_->IsVisibleOnAllWorkspaces();
  if (IsAssignToAllDesksCommand(command_id))
    return assigned_to_all_desks;

  return !assigned_to_all_desks &&
         MapCommandIdToDeskIndex(command_id) ==
             DesksHelper::Get(widget_->GetNativeWindow())->GetActiveDeskIndex();
}

bool MoveToDesksMenuDelegate::IsCommandIdEnabled(int command_id) const {
  if (IsAssignToAllDesksCommand(command_id)) {
    return true;
  }

  if (!IsMoveToDeskCommand(command_id))
    return false;

  return MapCommandIdToDeskIndex(command_id) <
         DesksHelper::Get(widget_->GetNativeWindow())->GetNumberOfDesks();
}

bool MoveToDesksMenuDelegate::IsCommandIdVisible(int command_id) const {
  return IsCommandIdEnabled(command_id);
}

bool MoveToDesksMenuDelegate::IsItemForCommandIdDynamic(int command_id) const {
  // The potential command_id is from MoveToDesksMenuModel::MOVE_TO_DESK_1
  // to MoveToDesksMenuModel::MOVE_TO_DESK_16,
  // MoveToDesksMenuModel::TOGGLE_ASSIGN_TO_ALL_DESKS.
  // For Move window to desk menu, all the menu items are dynamic.
  // Therefore, checking whether command_id is within the range from
  // MOVE_TO_DESK_1 to TOGGLE_ASSIGN_TO_ALL_DESKS
  return chromeos::MoveToDesksMenuModel::MOVE_TO_DESK_1 <= command_id &&
         command_id <=
             chromeos::MoveToDesksMenuModel::TOGGLE_ASSIGN_TO_ALL_DESKS;
}

std::u16string MoveToDesksMenuDelegate::GetLabelForCommandId(
    int command_id) const {
  if (IsAssignToAllDesksCommand(command_id))
    return l10n_util::GetStringUTF16(IDS_ASSIGN_TO_ALL_DESKS);

  // It gets desk name for all the desks, and desk items are all dynamic here.
  // Therefore, for the desk a user adds, it returns the name of the desk.
  // Otherwise, the desk name is empty string.
  return DesksHelper::Get(widget_->GetNativeWindow())
      ->GetDeskName(MapCommandIdToDeskIndex(command_id));
}

void MoveToDesksMenuDelegate::ExecuteCommand(int command_id, int event_flags) {
  if (!IsAssignToAllDesksCommand(command_id)) {
    DesksHelper::Get(widget_->GetNativeWindow())
        ->SendToDeskAtIndex(widget_->GetNativeWindow(),
                            MapCommandIdToDeskIndex(command_id));
    return;
  }

  const bool was_visible_on_all_desks = widget_->IsVisibleOnAllWorkspaces();
  if (!was_visible_on_all_desks) {
    UMA_HISTOGRAM_ENUMERATION(
        chromeos::kDesksAssignToAllDesksSourceHistogramName,
        chromeos::DesksAssignToAllDesksSource::kMoveToDeskMenu);
  }
  widget_->SetVisibleOnAllWorkspaces(!was_visible_on_all_desks);
}

}  // namespace chromeos
