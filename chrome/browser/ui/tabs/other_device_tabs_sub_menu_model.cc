// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/other_device_tabs_sub_menu_model.h"

#include <memory>
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
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
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
  // Adds the tab row to `model`, binds its open callback, and fetches its
  // favicon.
  auto add_tab = [this](ui::SimpleMenuModel& model, const RecentTabItem& tab) {
    const int command_id = GetAndIncrementNextMenuID();
    model.AddItem(command_id, tab.title());
    model.SetExecuteCallbackAt(
        model.GetItemCount() - 1,
        base::BindRepeating(&OtherDeviceTabsSubMenuModel::OpenTab,
                            base::Unretained(this), tab.session_tag(),
                            tab.session_id()));
    remote_tabs_menu_util::AddRemoteTabFavicon(
        browser_->GetProfile(), favicon_weak_ptr_factory_.GetWeakPtr(), &model,
        command_id, tab.url());
  };

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
      case RecentTabItem::Type::kTab:
        add_tab(*this, item);
        break;
      case RecentTabItem::Type::kMoreTabs: {
        // Flyout with the device's tabs that don't fit inline.
        ui::SimpleMenuModel& more_tabs = *more_tabs_sub_menus_.emplace_back(
            std::make_unique<ui::SimpleMenuModel>(this));
        for (const RecentTabItem& tab : item.children()) {
          add_tab(more_tabs, tab);
        }
        AddSubMenuWithIcon(GetAndIncrementNextMenuID(), item.title(),
                           &more_tabs, item.icon());
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
      ui::DispositionFromEventFlags(
          event_flags, ui::mojom::WindowOpenDisposition::kNewForegroundTab),
      base::UserMetricsAction("WrenchMenu_OpenRecentTabFromDevice"));
}

int OtherDeviceTabsSubMenuModel::GetAndIncrementNextMenuID() {
  // Stagger command IDs across unbounded menu types to prevent collisions.
  const int current_id = next_menu_id_;
  next_menu_id_ += AppMenuModel::kNumUnboundedMenuTypes;
  return current_id;
}
