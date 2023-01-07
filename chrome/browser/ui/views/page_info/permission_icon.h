// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_ICON_H_

#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "components/page_info/page_info_ui.h"
#include "third_party/skia/include/core/SkColor.h"

class PermissionIcon : public NonAccessibleImageView {
 public:
  explicit PermissionIcon(const PageInfo::PermissionInfo& permission_info);

  PermissionIcon(const PermissionIcon&) = delete;
  PermissionIcon& operator=(const PermissionIcon&) = delete;

  ~PermissionIcon() override = default;

  void OnPermissionChanged(const PageInfo::PermissionInfo& permission_info);

 private:
  PageInfo::PermissionInfo permission_info_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_ICON_H_
