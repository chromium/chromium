// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/mojo_menu_model.h"

#include "ui/base/models/image_model.h"

namespace remote_cocoa {

MojoMenuModel::MojoMenuModel(std::vector<mojom::MenuItemPtr> menu_items,
                             mojom::MenuHost* menu_host)
    : menu_items_(std::move(menu_items)), menu_host_(menu_host) {
  submenus_.resize(menu_items_.size());
}

MojoMenuModel::~MojoMenuModel() = default;

base::WeakPtr<ui::MenuModel> MojoMenuModel::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t MojoMenuModel::GetItemCount() const {
  return menu_items_.size();
}

ui::MenuModel::ItemType MojoMenuModel::GetTypeAt(size_t index) const {
  switch (menu_items_[index]->which()) {
    case mojom::MenuItem::Tag::kSeparator:
      return TYPE_SEPARATOR;
    case mojom::MenuItem::Tag::kRegular:
      return TYPE_COMMAND;
    case mojom::MenuItem::Tag::kSubmenu:
      return TYPE_SUBMENU;
  }
  NOTREACHED_IN_MIGRATION();
}

ui::MenuSeparatorType MojoMenuModel::GetSeparatorTypeAt(size_t index) const {
  // Not used by MenuControllerCocoa.
  return ui::NORMAL_SEPARATOR;
}

int MojoMenuModel::GetCommandIdAt(size_t index) const {
  return GetCommonFieldsAt(index)->command_id;
}

std::u16string MojoMenuModel::GetLabelAt(size_t index) const {
  return GetCommonFieldsAt(index)->label;
}

bool MojoMenuModel::IsItemDynamicAt(size_t index) const {
  // For now we don't support dynamic menu items. This doesn't matter since only
  // dynamic items in submenus would be meaningful anyway (as a MojoMenuModel is
  // never displayed more than once, so the top-level menu can't be shown
  // multiple times).
  return false;
}

bool MojoMenuModel::MayHaveMnemonicsAt(size_t index) const {
  return GetCommonFieldsAt(index)->may_have_mnemonics;
}

bool MojoMenuModel::GetAcceleratorAt(size_t index,
                                     ui::Accelerator* accelerator) const {
  // On macOS context menus never have accelerators. Since this class is only
  // used for context menus, this always returns false.
  return false;
}

bool MojoMenuModel::IsItemCheckedAt(size_t index) const {
  return GetCommonFieldsAt(index)->is_checked;
}

int MojoMenuModel::GetGroupIdAt(size_t index) const {
  // Not used by MenuControllerCocoa.
  return -1;
}

ui::ImageModel MojoMenuModel::GetIconAt(size_t index) const {
  if (const auto& icon = GetCommonFieldsAt(index)->icon; !icon.isNull()) {
    return ui::ImageModel::FromImageSkia(icon);
  }
  return {};
}

ui::ButtonMenuItemModel* MojoMenuModel::GetButtonMenuItemAt(
    size_t index) const {
  // Not used by MenuControllerCocoa.
  return nullptr;
}

bool MojoMenuModel::IsEnabledAt(size_t index) const {
  return GetCommonFieldsAt(index)->is_enabled;
}

bool MojoMenuModel::IsVisibleAt(size_t index) const {
  return GetCommonFieldsAt(index)->is_visible;
}

bool MojoMenuModel::IsAlertedAt(size_t index) const {
  return GetCommonFieldsAt(index)->is_alerted;
}

bool MojoMenuModel::IsNewFeatureAt(size_t index) const {
  return GetCommonFieldsAt(index)->is_new_feature;
}

MojoMenuModel* MojoMenuModel::GetSubmenuModelAt(size_t index) const {
  if (!menu_items_[index]->is_submenu()) {
    return nullptr;
  }
  std::unique_ptr<MojoMenuModel>& submenu = submenus_[index];
  if (!submenu) {
    submenu = std::make_unique<MojoMenuModel>(
        std::move(menu_items_[index]->get_submenu()->children), menu_host_);
  }
  return submenu.get();
}

void MojoMenuModel::ActivatedAt(size_t index) {
  ActivatedAt(index, /*event_flags=*/0);
}

void MojoMenuModel::ActivatedAt(size_t index, int event_flags) {
  menu_host_->CommandActivated(GetCommonFieldsAt(index)->command_id,
                               event_flags);
}

mojom::MenuItemCommonFields* MojoMenuModel::GetCommonFieldsAt(
    size_t index) const {
  switch (menu_items_[index]->which()) {
    case mojom::MenuItem::Tag::kSeparator:
      return menu_items_[index]->get_separator().get();
    case mojom::MenuItem::Tag::kRegular:
      return menu_items_[index]->get_regular().get();
    case mojom::MenuItem::Tag::kSubmenu:
      return menu_items_[index]->get_submenu()->common.get();
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace remote_cocoa
