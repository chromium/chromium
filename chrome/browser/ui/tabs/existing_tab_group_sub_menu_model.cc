// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"

#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

constexpr int kFirstCommandIndex =
    TabStripModel::ContextMenuCommand::CommandLast + 1;

ExistingTabGroupSubMenuModel::ExistingTabGroupSubMenuModel(TabStripModel* model,
                                                           int context_index)
    : SimpleMenuModel(this) {
  model_ = model;
  context_index_ = context_index;
  Build();
}

void ExistingTabGroupSubMenuModel::Build() {
  // Start command ids after the parent menu's ids to avoid collisions.
  int group_index = kFirstCommandIndex;
  for (TabGroupId group : model_->ListTabGroups()) {
    if (ShouldShowGroup(model_, context_index_, group))
      AddItem(group_index, model_->GetUserVisibleGroupTitle(group));
    group_index++;
  }
}

bool ExistingTabGroupSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ExistingTabGroupSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void ExistingTabGroupSubMenuModel::ExecuteCommand(int command_id,
                                                  int event_flags) {
  const int group_index = command_id - kFirstCommandIndex;
  // TODO(https://crbug.com/922736): If a group has been deleted, |group_index|
  // may refer to a different group than it did when the menu was created.
  DCHECK_LT(size_t{group_index}, model_->ListTabGroups().size());
  model_->ExecuteAddToExistingGroupCommand(
      context_index_, model_->ListTabGroups()[group_index]);
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowSubmenu(TabStripModel* model,
                                                     int context_index) {
  for (TabGroupId group : model->ListTabGroups()) {
    if (ShouldShowGroup(model, context_index, group)) {
      return true;
    }
  }
  return false;
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowGroup(TabStripModel* model,
                                                   int context_index,
                                                   TabGroupId group) {
  if (!model->IsTabSelected(context_index)) {
    if (group != model->GetTabGroupForTab(context_index))
      return true;
  } else {
    for (int index : model->selection_model().selected_indices()) {
      if (group != model->GetTabGroupForTab(index)) {
        return true;
      }
    }
  }
  return false;
}
