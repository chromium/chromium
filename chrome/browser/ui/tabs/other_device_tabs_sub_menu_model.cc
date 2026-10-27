// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/other_device_tabs_sub_menu_model.h"

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/remote_tabs_menu_util.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"

OtherDeviceTabsSubMenuModel::OtherDeviceTabsSubMenuModel(
    BrowserWindowInterface* browser)
    : ui::SimpleMenuModel(this),
      browser_(browser),
      next_menu_id_(AppMenuModel::kMinOtherDeviceTabsCommandId) {
  Build();
}

OtherDeviceTabsSubMenuModel::~OtherDeviceTabsSubMenuModel() = default;

void OtherDeviceTabsSubMenuModel::ExecuteCommand(int command_id,
                                                 int event_flags) {}

void OtherDeviceTabsSubMenuModel::Build() {
  // Populates device headers, open tabs, and separators ordered by recency.
  for (const RecentTabItem& item :
       RecentTabsBuilder::BuildOtherDeviceTabs(browser_->GetProfile())) {
    switch (item.type()) {
      case RecentTabItem::Type::kDivider:
        // Separator between consecutive devices.
        AddSeparator(ui::NORMAL_SEPARATOR);
        break;
      case RecentTabItem::Type::kHeader:
        // Non-interactive device name header.
        AddTitle(item.title());
        break;
      case RecentTabItem::Type::kTab: {
        // Add the tab row and asynchronously fetch its favicon.
        const int command_id = GetAndIncrementNextMenuID();
        AddItem(command_id, item.title());
        remote_tabs_menu_util::AddRemoteTabFavicon(
            browser_->GetProfile(), favicon_weak_ptr_factory_.GetWeakPtr(),
            this, command_id, item.url());
        break;
      }
      default:
        NOTREACHED();
    }
  }
}

int OtherDeviceTabsSubMenuModel::GetAndIncrementNextMenuID() {
  // Stagger command IDs across unbounded menu types to prevent collisions.
  const int current_id = next_menu_id_;
  next_menu_id_ += AppMenuModel::kNumUnboundedMenuTypes;
  return current_id;
}
