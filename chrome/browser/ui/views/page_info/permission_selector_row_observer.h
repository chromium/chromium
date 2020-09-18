// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_SELECTOR_ROW_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_SELECTOR_ROW_OBSERVER_H_

#include "components/page_info/page_info_ui.h"

class PermissionSelectorRowObserver {
 public:
  // This method is called whenever the permission setting is changed.
  virtual void OnPermissionChanged(
      const PageInfo::PermissionInfo& permission) = 0;

 protected:
  virtual ~PermissionSelectorRowObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_SELECTOR_ROW_OBSERVER_H_
