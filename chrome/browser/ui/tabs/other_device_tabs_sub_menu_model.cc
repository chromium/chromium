// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/other_device_tabs_sub_menu_model.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/remote_tabs_menu_util.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "components/sync_sessions/session_sync_service.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"

OtherDeviceTabsSubMenuModel::OtherDeviceTabsSubMenuModel(
    BrowserWindowInterface* browser,
    LogMenuMetricsCallback log_menu_metrics_callback)
    : ui::SimpleMenuModel(this),
      browser_(browser),
      log_menu_metrics_callback_(std::move(log_menu_metrics_callback)),
      session_sync_service_(
          SessionSyncServiceFactory::GetForProfile(browser->GetProfile())),
      next_menu_id_(AppMenuModel::kMinOtherDeviceTabsCommandId) {
  CHECK(log_menu_metrics_callback_);
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
        // Add the tab row, bind its open callback, and fetch its favicon.
        const int command_id = GetAndIncrementNextMenuID();
        AddItem(command_id, item.title());
        SetExecuteCallbackAt(
            GetItemCount() - 1,
            base::BindRepeating(&OtherDeviceTabsSubMenuModel::OpenTab,
                                base::Unretained(this), item.session_tag(),
                                item.session_id()));
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

void OtherDeviceTabsSubMenuModel::OpenTab(const std::string& session_tag,
                                          SessionID tab_id,
                                          int event_flags) {
  log_menu_metrics_callback_.Run(IDC_OPEN_OTHER_DEVICE_TAB);

  // Restore the selected foreign tab using the window disposition derived
  // from event flags (e.g. foreground vs. background tab on middle-click).
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      session_sync_service_ ? session_sync_service_->GetOpenTabsUIDelegate()
                            : nullptr;
  remote_tabs_menu_util::OpenForeignSessionTab(
      browser_, open_tabs, session_tag, tab_id,
      ui::DispositionFromEventFlags(event_flags,
                                    WindowOpenDisposition::kNewForegroundTab),
      base::UserMetricsAction("WrenchMenu_OpenRecentTabFromDevice"));
}

int OtherDeviceTabsSubMenuModel::GetAndIncrementNextMenuID() {
  // Stagger command IDs across unbounded menu types to prevent collisions.
  const int current_id = next_menu_id_;
  next_menu_id_ += AppMenuModel::kNumUnboundedMenuTypes;
  return current_id;
}
