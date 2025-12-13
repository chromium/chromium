// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_GROUP_MENU_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_TAB_GROUP_MENU_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/main_menu_item.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

using TabGroupMenuAction = tab_groups::TabGroupMenuAction;

@class MenuItemListener;
class Profile;

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

class TabGroupMenuBridge : public MainMenuItem,
                           public tab_groups::TabGroupSyncService::Observer {
 public:
  explicit TabGroupMenuBridge(
      Profile* profile,
      tab_groups::TabGroupSyncService* tab_group_service);
  ~TabGroupMenuBridge() override;

  // MainMenuItem:
  void ResetMenu() override;
  void BuildMenu() override;

  // tab_groups::TabGroupSyncService::Observer:
  void OnInitialized() override;
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         tab_groups::TriggerSource source) override;

 protected:
  virtual NSMenu* TabGroupsMenu();

 private:
  // Callback to invoke when favicon is loaded.
  void OnFaviconReady(NSMenuItem* menu_item,
                      const favicon_base::FaviconImageResult& result);

  // Helper function to create a static submenu item for a tab group.
  NSMenuItem* CreateStaticSubmenuItem(int string_id,
                                      TabGroupMenuAction::Type type,
                                      const base::Uuid& uuid);

  // Callback to invoke when menu item is clicked.
  void OnMenuItem(NSMenuItem* item);

  const raw_ptr<Profile> profile_;
  const raw_ptr<tab_groups::TabGroupSyncService> tab_group_service_;
  const raw_ptr<favicon::FaviconService> favicon_service_;

  // Make sure favicon requests are cancelled when the menu is recreated or the
  // class is destroyed.
  base::CancelableTaskTracker favicon_tracker_;

  // Observer to listen for tab group change.
  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      observation_{this};

  // Menu listener to handle click action.
  MenuItemListener* __strong menu_listener_;

  // Map menu item to action.
  std::map<NSMenuItem*, TabGroupMenuAction> menu_item_map_;
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_GROUP_MENU_BRIDGE_H_
