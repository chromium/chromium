// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/concat_menu_model.h"

ConcatMenuModel::ConcatMenuModel(ui::MenuModel* m1, ui::MenuModel* m2)
    : m1_(m1), m2_(m2) {}

ConcatMenuModel::~ConcatMenuModel() = default;

bool ConcatMenuModel::HasIcons() const {
  return m1_->HasIcons() || m2_->HasIcons();
}

int ConcatMenuModel::GetItemCount() const {
  return m1_->GetItemCount() + m2_->GetItemCount();
}

ui::MenuModel::ItemType ConcatMenuModel::GetTypeAt(int index) const {
  return GetterImpl(&ui::MenuModel::GetTypeAt, index);
}

ui::MenuSeparatorType ConcatMenuModel::GetSeparatorTypeAt(int index) const {
  return GetterImpl(&ui::MenuModel::GetSeparatorTypeAt, index);
}

int ConcatMenuModel::GetCommandIdAt(int index) const {
  return GetterImpl(&ui::MenuModel::GetCommandIdAt, index);
}

base::string16 ConcatMenuModel::GetLabelAt(int index) const {
  return GetterImpl(&ui::MenuModel::GetLabelAt, index);
}

base::string16 ConcatMenuModel::GetMinorTextAt(int index) const {
  return GetterImpl(&ui::MenuModel::GetMinorTextAt, index);
}

const gfx::VectorIcon* ConcatMenuModel::GetMinorIconAt(int index) const {
  return GetterImpl(&ui::MenuModel::GetMinorIconAt, index);
}

bool ConcatMenuModel::IsItemDynamicAt(int index) const {
  return GetterImpl(&ui::MenuModel::IsItemDynamicAt, index);
}

bool ConcatMenuModel::GetAcceleratorAt(int index,
                                       ui::Accelerator* accelerator) const {
  return GetterImpl(&ui::MenuModel::GetAcceleratorAt, index, accelerator);
}

bool ConcatMenuModel::IsItemCheckedAt(int index) const {
  return GetterImpl(&ui::MenuModel::IsItemCheckedAt, index);
}

int ConcatMenuModel::GetGroupIdAt(int index) const {
  return GetterImpl(&ui::MenuModel::GetGroupIdAt, index);
}

bool ConcatMenuModel::GetIconAt(int index, gfx::Image* icon) const {
  return GetterImpl(&ui::MenuModel::GetGroupIdAt, index);
}

ui::ButtonMenuItemModel* ConcatMenuModel::GetButtonMenuItemAt(int index) const {
  return GetterImpl(&ui::MenuModel::GetButtonMenuItemAt, index);
}

bool ConcatMenuModel::IsEnabledAt(int index) const {
  return GetterImpl(&ui::MenuModel::IsEnabledAt, index);
}

bool ConcatMenuModel::IsVisibleAt(int index) const {
  return GetterImpl(&ui::MenuModel::IsVisibleAt, index);
}

void ConcatMenuModel::ActivatedAt(int index) {
  GetMenuAndIndex(&index)->ActivatedAt(index);
}

void ConcatMenuModel::ActivatedAt(int index, int event_flags) {
  GetMenuAndIndex(&index)->ActivatedAt(index, event_flags);
}

ui::MenuModel* ConcatMenuModel::GetSubmenuModelAt(int index) const {
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

ui::MenuModel* ConcatMenuModel::GetMenuAndIndex(int* index) const {
  int m1_count = m1_->GetItemCount();
  if (*index < m1_count)
    return m1_;

  DCHECK_LT(*index - m1_count, m2_->GetItemCount());
  *index -= m1_count;
  return m2_;
}
