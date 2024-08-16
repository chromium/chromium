// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/concat_menu_model.h"

ConcatMenuModel::ConcatMenuModel(ui::MenuModel* m1, ui::MenuModel* m2)
    : m1_(m1), m2_(m2) {}

ConcatMenuModel::~ConcatMenuModel() = default;

base::WeakPtr<ui::MenuModel> ConcatMenuModel::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t ConcatMenuModel::GetItemCount() const {
  return m1_->GetItemCount() + m2_->GetItemCount();
}

ui::MenuModel::ItemType ConcatMenuModel::GetTypeAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetTypeAt, index);
}

ui::MenuSeparatorType ConcatMenuModel::GetSeparatorTypeAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetSeparatorTypeAt, index);
}

int ConcatMenuModel::GetCommandIdAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetCommandIdAt, index);
}

std::u16string ConcatMenuModel::GetLabelAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetLabelAt, index);
}

std::u16string ConcatMenuModel::GetMinorTextAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetMinorTextAt, index);
}

ui::ImageModel ConcatMenuModel::GetMinorIconAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetMinorIconAt, index);
}

bool ConcatMenuModel::IsItemDynamicAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::IsItemDynamicAt, index);
}

bool ConcatMenuModel::GetAcceleratorAt(size_t index,
                                       ui::Accelerator* accelerator) const {
  return GetterImpl(&ui::MenuModel::GetAcceleratorAt, index, accelerator);
}

bool ConcatMenuModel::IsItemCheckedAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::IsItemCheckedAt, index);
}

int ConcatMenuModel::GetGroupIdAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetGroupIdAt, index);
}

ui::ImageModel ConcatMenuModel::GetIconAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetIconAt, index);
}

ui::ButtonMenuItemModel* ConcatMenuModel::GetButtonMenuItemAt(
    size_t index) const {
  return GetterImpl(&ui::MenuModel::GetButtonMenuItemAt, index);
}

bool ConcatMenuModel::IsEnabledAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::IsEnabledAt, index);
}

bool ConcatMenuModel::IsVisibleAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::IsVisibleAt, index);
}

void ConcatMenuModel::ActivatedAt(size_t index) {
  GetMenuAndIndex(&index)->ActivatedAt(index);
}

void ConcatMenuModel::ActivatedAt(size_t index, int event_flags) {
  GetMenuAndIndex(&index)->ActivatedAt(index, event_flags);
}

ui::MenuModel* ConcatMenuModel::GetSubmenuModelAt(size_t index) const {
  return GetterImpl(&ui::MenuModel::GetSubmenuModelAt, index);
}

void ConcatMenuModel::MenuWillShow() {
  m1_->MenuWillShow();
  m2_->MenuWillShow();
}

void ConcatMenuModel::MenuWillClose() {
  m1_->MenuWillClose();
  m2_->MenuWillClose();
}

ui::MenuModel* ConcatMenuModel::GetMenuAndIndex(size_t* index) const {
  size_t m1_count = m1_->GetItemCount();
  if (*index < m1_count)
    return m1_;

  *index -= m1_count;
  DCHECK_LT(*index, m2_->GetItemCount());
  return m2_;
}
