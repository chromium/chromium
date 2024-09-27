// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_RECENT_TABS_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TABS_RECENT_TABS_SUB_MENU_MODEL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/sync_sessions/synced_session.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace favicon_base {
struct FaviconImageResult;
}

namespace sessions {
struct SessionTab;
}

namespace sync_sessions {
class OpenTabsUIDelegate;
class SessionSyncService;
}  // namespace sync_sessions

namespace ui {
class AcceleratorProvider;
}

// A menu model that builds the contents of "Recent tabs" submenu, which include
// the recently closed tabs/groups/windows of current device i.e. local entries,
// and opened tabs of other devices.
class RecentTabsSubMenuModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate,
                               public sessions::TabRestoreServiceObserver {
 public:
  using LogMenuMetricsCallback = base::RepeatingCallback<void(int)>;

  static int GetDisabledRecentlyClosedHeaderCommandId();

  // Exposed for tests only: return the Command Id for the first entry in the
  // recently closed window items list.
  int GetFirstRecentTabsCommandId();

  RecentTabsSubMenuModel(ui::AcceleratorProvider* accelerator_provider,
                         Browser* browser);

  RecentTabsSubMenuModel(const RecentTabsSubMenuModel&) = delete;
  RecentTabsSubMenuModel& operator=(const RecentTabsSubMenuModel&) = delete;

  ~RecentTabsSubMenuModel() override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  void RegisterLogMenuMetricsCallback(LogMenuMetricsCallback callback);

 private:
  // The various command types this menu supports.
  enum CommandType {
    Tab,          // Includes local and remote tabs.
    LocalTab,     // Tabs originating from this device.
    RemoteTab,    // Tabs originating from other devices.
    Group,        // Tab groups and their tabs.
    Window,       // Windows including their groups and tabs.
    Submenu,      // Items in the submenus such as separators.
    OtherDevice,  // Other devices.
  };

  struct TabItem;
  using TabItems = std::map<int, TabItem>;
  using WindowItems = std::map<int, SessionID>;
  using GroupItems = std::map<int, SessionID>;
  struct SubMenuItem;
  using SubMenuItems = std::map<int, SubMenuItem>;
  using DeviceNameItems = base::flat_set<int>;

  // Index of the separator that follows the history menu item. Used as a
  // reference position for inserting local entries.
  size_t history_separator_index_ = 0;

  // Build the menu items by populating the menumodel.
  void Build();

  // Build the recently closed tabs and windows items.
  void BuildLocalEntries();

  // Build the tabs items from other devices.
  void BuildTabsFromOtherDevices();

  // Build a recently closed tab item with parameters needed to restore it, and
  // add it to the menumodel at |curr_model_index|.
  void BuildLocalTabItem(const sessions::tab_restore::Tab& tab,
                         size_t curr_model_index);

  // Build the recently closed window item with parameters needed to restore it,
  // and add it to the menumodel at |curr_model_index|.
  void BuildLocalWindowItem(const sessions::tab_restore::Window& window,
                            size_t curr_model_index);

  // Build the recently closed group item with parameters needed to restore it,
  // and add it to the menumodel at |curr_model_index|.
  void BuildLocalGroupItem(const sessions::tab_restore::Group& group,
                           size_t curr_model_index);

  // Build the tab item for other devices with parameters needed to restore it.
  void BuildOtherDevicesTabItem(SimpleMenuModel* containing_model,
                                const std::string& session_tag,
                                const sessions::SessionTab& tab);

  // Build a sub menu model for given device session.
  std::unique_ptr<ui::SimpleMenuModel> CreateOtherDeviceSubMenu(
      const sync_sessions::SyncedSession* session,
      const std::vector<const sessions::SessionTab*>& tabs_in_session);

  // Create a submenu model representing the tabs within a window.
  std::unique_ptr<ui::SimpleMenuModel> CreateWindowSubMenuModel(
      const sessions::tab_restore::Window& window);

  // Create a submenu model representing the tabs within a tab group.
  std::unique_ptr<ui::SimpleMenuModel> CreateGroupSubMenuModel(
      const sessions::tab_restore::Group& group);

  // Adds a submenu item representation of |group_model| to |parent_model|.
  void AddGroupItemToModel(SimpleMenuModel* parent_model,
                           std::unique_ptr<SimpleMenuModel> group_model,
                           const sessions::tab_restore::Group& group);

  // Adds a submenu item representation of a |tab| to |model|.
  void AddTabItemToModel(const sessions::tab_restore::Tab* tab,
                         ui::SimpleMenuModel* model,
                         int command_id);

  // Return the appropriate menu item label for a tab group, given its title
  // and the number of tabs it contains.
  std::u16string GetGroupItemLabel(std::u16string title, size_t num_tabs);

  // Return the command id of the given id's parent submenu, if it has one that
  // is created by this menu model. Otherwise, return -1. This will be the case
  // for all items whose parent is the RecentTabsSubMenuModel itself.
  int GetParentCommandId(int command_id) const;

  // Add the favicon for the device section header.
  void AddDeviceFavicon(SimpleMenuModel* containing_model,
                        size_t index_in_menu,
                        syncer::DeviceInfo::FormFactor device_form_factor);

  // Add the favicon for a local or other devices' tab asynchronously,
  // OnFaviconDataAvailable() will be invoked when the favicon is ready.
  void AddTabFavicon(int command_id,
                     ui::SimpleMenuModel* menu_model,
                     const GURL& url);
  void OnFaviconDataAvailable(
      int command_id,
      ui::SimpleMenuModel* menu_model,
      const favicon_base::FaviconImageResult& image_result);

  // Clear all recently closed tabs and windows.
  void ClearLocalEntries();

  // Clears all tabs from other devices.
  void ClearTabsFromOtherDevices();

  // Returns the corresponding local or other devices' TabItems in
  // |local_tab_items_| or |remote_tab_items_|.
  TabItems* GetTabVectorForCommandId(int command_id);

  // Convenience function to access OpenTabsUIDelegate provided by
  // SessionSyncService. Can return null if session sync is not running.
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate();

  // Overridden from TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

  void OnForeignSessionUpdated();

  // Returns |next_menu_id_| and increments it by 2. This allows for 'sharing'
  // command ids with the bookmarks menu, which also uses every other int as
  // an id.
  int GetAndIncrementNextMenuID();

  // Executes the commands for entries that do something other than restore tabs
  // such as the "History", "Group History", and "Sign in to see tabs from other
  // devices" menu items. Returns true if `command_id` was executed.
  bool ExecuteCustomCommand(int command_id, int event_flags);

  // Returns true if `command_id` identifies as `command_type`.
  bool IsCommandType(CommandType command_type, int command_id) const;

  const raw_ptr<Browser> browser_;  // Weak.

  LogMenuMetricsCallback log_menu_metrics_callback_;

  const raw_ptr<sync_sessions::SessionSyncService>
      session_sync_service_;  // Weak.

  // Accelerator for reopening last closed tab.
  ui::Accelerator reopen_closed_tab_accelerator_;

  // Accelerator for showing history.
  ui::Accelerator show_history_accelerator_;

  // ID of the next menu item.
  int next_menu_id_;

  // Tab items for local recently closed tabs.
  TabItems local_tab_items_;

  // Similar to `local_tab_items_` except the tabs are opened on remote devices.
  TabItems remote_tab_items_;

  // Window items for local recently closed windows.
  WindowItems local_window_items_;

  // Group items for local recently closed groups.
  GroupItems local_group_items_;

  // Sub menu items for sub menu entry points representing local recently
  // closed groups and windows. These are not executable.
  SubMenuItems local_sub_menu_items_;

  // Menu items representing other devices.
  SubMenuItems remote_sub_menu_items_;

  // Index of "Recent tabs" title item.
  std::optional<size_t> recent_tabs_title_index_;

  // Index of the last local entry (recently closed tab, window, or group) in
  // the menumodel.
  size_t last_local_model_index_ = 0;

  base::CancelableTaskTracker local_tab_cancelable_task_tracker_;

  base::ScopedObservation<sessions::TabRestoreService,
                          sessions::TabRestoreServiceObserver>
      tab_restore_service_observation_{this};

  base::CallbackListSubscription foreign_session_updated_subscription_;

  base::WeakPtrFactory<RecentTabsSubMenuModel> weak_ptr_factory_{this};
  base::WeakPtrFactory<RecentTabsSubMenuModel>
      weak_ptr_factory_for_other_devices_tab_{this};
};

#endif  // CHROME_BROWSER_UI_TABS_RECENT_TABS_SUB_MENU_MODEL_H_
