// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_base_sub_menu_model.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/base/resource/resource_bundle.h"

ExistingBaseSubMenuModel::ExistingBaseSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabStripModel* model,
    int context_index,
    int min_command_id,
    int parent_new_command_id)
    : SimpleMenuModel(this),
      parent_delegate_(parent_delegate),
      model_(model),
      context_contents_(model->GetWebContentsAt(context_index)),
      min_command_id_(min_command_id),
      parent_new_command_id_(parent_new_command_id) {}

bool ExistingBaseSubMenuModel::IsCommandIdAlerted(int command_id) const {
  return IsNewCommand(command_id) &&
         parent_delegate()->IsCommandIdAlerted(parent_new_command_id_);
}

constexpr int ExistingBaseSubMenuModel::kMinExistingWindowCommandId;
constexpr int ExistingBaseSubMenuModel::kMinExistingTabGroupCommandId;

void ExistingBaseSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (IsNewCommand(command_id)) {
    parent_delegate()->ExecuteCommand(parent_new_command_id_, event_flags);
    return;
  }
  ExecuteExistingCommand(command_id_to_target_index_[command_id]);
}

ExistingBaseSubMenuModel::~ExistingBaseSubMenuModel() = default;

ExistingBaseSubMenuModel::MenuItemInfo::MenuItemInfo(
    const std::u16string menu_text)
    : text(menu_text) {
  image = std::nullopt;
}

ExistingBaseSubMenuModel::MenuItemInfo::MenuItemInfo(
    const std::u16string& menu_text,
    ui::ImageModel menu_image)
    : text(menu_text) {
  image = std::optional<ui::ImageModel>{menu_image};
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
  int command_id = min_command_id_ + 1;
  for (size_t i = 0; i < menu_item_infos.size(); ++i) {
    const MenuItemInfo& item = menu_item_infos[i];
    if (command_id > min_command_id_ + static_cast<int>(max_size))
      break;

    if (item.target_index.has_value()) {
      command_id_to_target_index_[command_id] = item.target_index.value();
      if (item.image.has_value()) {
        AddItemWithIcon(command_id++, item.text, item.image.value());
      } else {
        AddItem(command_id++, item.text);
      }
    } else {
      // Add a spacing separator to further delineate menu item groupings.
      if (i > 0)
        AddSeparator(ui::SPACING_SEPARATOR);

      AddTitle(item.text);
    }

    SetAccessibleNameAt(GetItemCount() - 1, item.accessible_name);
    SetMayHaveMnemonicsAt(GetItemCount() - 1, item.may_have_mnemonics);
  }
}

void ExistingBaseSubMenuModel::ClearMenu() {
  Clear();
  command_id_to_target_index_.clear();
}

bool ExistingBaseSubMenuModel::IsNewCommand(int command_id) const {
  return command_id == min_command_id_;
}

void ExistingBaseSubMenuModel::ExecuteExistingCommand(size_t target_index) {}

int ExistingBaseSubMenuModel::GetContextIndex() const {
  return model_->GetIndexOfWebContents(context_contents_);
}
