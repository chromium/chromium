// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sessions/core/session_id.h"
#include "ui/menus/simple_menu_model.h"

class BrowserWindowInterface;

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

// The "Other device tabs" app menu submenu: open tabs from the user's other
// devices, listed inline under device title headers ordered by recency. What to
// show and the items themselves come from `RecentTabsBuilder`; this class only
// renders them as a `ui::SimpleMenuModel`.
class OtherDeviceTabsSubMenuModel : public ui::SimpleMenuModel,
                                    public ui::SimpleMenuModel::Delegate {
 public:
  // Callback invoked to log menu metrics. The `int` argument passed to this
  // callback is a command ID (e.g. `IDC_OPEN_OTHER_DEVICE_TAB`).
  using LogMenuMetricsCallback = base::RepeatingCallback<void(int command_id)>;

  OtherDeviceTabsSubMenuModel(BrowserWindowInterface* browser,
                              LogMenuMetricsCallback log_menu_metrics_callback);
  OtherDeviceTabsSubMenuModel(const OtherDeviceTabsSubMenuModel&) = delete;
  OtherDeviceTabsSubMenuModel& operator=(const OtherDeviceTabsSubMenuModel&) =
      delete;
  ~OtherDeviceTabsSubMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Populates the menu for the current state of `browser_`'s profile.
  void Build();

  // Rebuilds the menu and notifies the delegate when foreign sessions change.
  void OnForeignSessionUpdated();

  void OpenTab(const std::string& session_tag,
               SessionID tab_id,
               int event_flags);

  // Returns the next dynamic command id. Ids are staggered by
  // `AppMenuModel::kNumUnboundedMenuTypes` so they never collide with the
  // other unbounded app menu submenus.
  int GetAndIncrementNextMenuID();

  const raw_ptr<BrowserWindowInterface> browser_;
  const LogMenuMetricsCallback log_menu_metrics_callback_;
  const raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;

  int next_menu_id_;

  // Flyouts listing the tabs of a device that don't fit inline.
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> more_tabs_sub_menus_;

  base::CallbackListSubscription foreign_session_updated_subscription_;

  // Invalidated on every rebuild so stale favicon callbacks are dropped.
  base::WeakPtrFactory<OtherDeviceTabsSubMenuModel> favicon_weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_TABS_OTHER_DEVICE_TABS_SUB_MENU_MODEL_H_
