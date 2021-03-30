// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"

ExistingBaseSubMenuModel::ExistingBaseSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabStripModel* model,
    int context_index,
    int min_command_id)
    : SimpleMenuModel(this),
      parent_delegate_(parent_delegate),
      model_(model),
      context_contents_(model->GetWebContentsAt(context_index)),
      min_command_id_(min_command_id) {}

bool ExistingBaseSubMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return false;
}

bool ExistingBaseSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ExistingBaseSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

constexpr int ExistingBaseSubMenuModel::kMinExistingWindowCommandId;
constexpr int ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId;

void ExistingBaseSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (IsNewCommand(command_id)) {
    ExecuteNewCommand(event_flags);
    return;
  }
  ExecuteExistingCommand(command_id - min_command_id_ - 1);
}

ExistingBaseSubMenuModel::~ExistingBaseSubMenuModel() = default;

ExistingBaseSubMenuModel::MenuItemInfo::MenuItemInfo(
    const std::u16string menu_text)
    : text(menu_text) {
  image = base::nullopt;
}

ExistingBaseSubMenuModel::MenuItemInfo::MenuItemInfo(
    const std::u16string& menu_text,
    ui::ImageModel menu_image)
    : text(menu_text) {
  image = base::Optional<ui::ImageModel>{menu_image};
}

ExistingBaseSubMenuModel::MenuItemInfo::MenuItemInfo(
    const MenuItemInfo& menu_item_info) = default;

ExistingBaseSubMenuModel::MenuItemInfo::~MenuItemInfo() = default;

void ExistingBaseSubMenuModel::Build(
    int new_text,
    std::vector<MenuItemInfo> menu_item_infos) {
  AddItemWithStringId(min_command_id_, new_text);
  AddSeparator(ui::NORMAL_SEPARATOR);

  // Start command ids after the parent menu's ids to avoid collisions.
  int group_index = min_command_id_ + 1;
  for (auto item : menu_item_infos) {
    if (group_index > min_command_id_ + max_size)
      break;

    if (item.image.has_value()) {
      AddItemWithIcon(group_index, item.text, item.image.value());
    } else {
      AddItem(group_index, item.text);
    }

    SetMayHaveMnemonicsAt(GetItemCount() - 1, item.may_have_mnemonics);
    group_index++;
  }
}

void ExistingBaseSubMenuModel::ExecuteNewCommand(int event_flags) {}

void ExistingBaseSubMenuModel::ExecuteExistingCommand(int command_index) {}

int ExistingBaseSubMenuModel::GetContextIndex() const {
  return model_->GetIndexOfWebContents(context_contents_);
}
