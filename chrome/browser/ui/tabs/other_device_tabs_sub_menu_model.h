// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/menus/simple_menu_model.h"

class BrowserWindowInterface;

// The "Other device tabs" app menu submenu: open tabs from the user's other
// devices, listed inline under device title headers ordered by recency. What to
// show and the items themselves come from `RecentTabsBuilder`; this class only
// renders them as a `ui::SimpleMenuModel`.
class OtherDeviceTabsSubMenuModel : public ui::SimpleMenuModel,
                                    public ui::SimpleMenuModel::Delegate {
 public:
  explicit OtherDeviceTabsSubMenuModel(BrowserWindowInterface* browser);
  OtherDeviceTabsSubMenuModel(const OtherDeviceTabsSubMenuModel&) = delete;
  OtherDeviceTabsSubMenuModel& operator=(const OtherDeviceTabsSubMenuModel&) =
      delete;
  ~OtherDeviceTabsSubMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Populates the menu for the current state of `browser_`'s profile.
  void Build();

  // Returns the next dynamic command id. Ids are staggered by
  // `AppMenuModel::kNumUnboundedMenuTypes` so they never collide with the
  // other unbounded app menu submenus.
  int GetAndIncrementNextMenuID();

  const raw_ptr<BrowserWindowInterface> browser_;

  int next_menu_id_;

  base::WeakPtrFactory<OtherDeviceTabsSubMenuModel> favicon_weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_
