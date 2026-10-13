// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_

#include "ui/menus/simple_menu_model.h"

// The "Other device tabs" app menu submenu: open tabs from the user's other
// devices.
class OtherDeviceTabsSubMenuModel : public ui::SimpleMenuModel,
                                    public ui::SimpleMenuModel::Delegate {
 public:
  OtherDeviceTabsSubMenuModel();
  OtherDeviceTabsSubMenuModel(const OtherDeviceTabsSubMenuModel&) = delete;
  OtherDeviceTabsSubMenuModel& operator=(const OtherDeviceTabsSubMenuModel&) =
      delete;
  ~OtherDeviceTabsSubMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
};

#endif  // CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_
