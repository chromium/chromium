// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_OBSERVER_H_

#include "components/page_info/page_info_ui.h"

class PermissionToggleRowViewObserver {
 public:
  // This method is called whenever the permission setting is changed.
  virtual void OnPermissionChanged(
      const PageInfo::PermissionInfo& permission) = 0;

 protected:
  virtual ~PermissionToggleRowViewObserver() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PERMISSION_TOGGLE_ROW_VIEW_OBSERVER_H_
