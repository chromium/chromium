// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/permission_icon.h"

#include "chrome/browser/ui/views/chrome_typography.h"

PermissionIcon::PermissionIcon(const PageInfo::PermissionInfo& permission_info)
    : permission_info_(permission_info) {}

void PermissionIcon::OnPermissionChanged(
    const PageInfo::PermissionInfo& permission_info) {
  permission_info_ = permission_info;
  UpdateImage();
}

void PermissionIcon::OnThemeChanged() {
  NonAccessibleImageView::OnThemeChanged();
  UpdateImage();
}

void PermissionIcon::UpdateImage() {
  SetImage(PageInfoUI::GetPermissionIcon(
      permission_info_,
      views::style::GetColor(*this, views::style::CONTEXT_LABEL,
                             views::style::STYLE_PRIMARY)));
}
