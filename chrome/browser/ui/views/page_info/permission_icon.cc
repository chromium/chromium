// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/permission_icon.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"

PermissionIcon::PermissionIcon(
    const PageInfo::PermissionInfo& permission_info) {
  OnPermissionChanged(permission_info);
}

void PermissionIcon::OnPermissionChanged(
    const PageInfo::PermissionInfo& permission_info) {
  permission_info_ = permission_info;
  SetImage(PageInfoViewFactory::GetPermissionIcon(permission_info_));
}
