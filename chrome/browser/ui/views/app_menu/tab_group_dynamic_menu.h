// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_TAB_GROUP_DYNAMIC_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_TAB_GROUP_DYNAMIC_MENU_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "components/favicon_base/favicon_types.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "ui/actions/actions.h"

class Profile;

namespace favicon {
class FaviconService;
}

// Class to support the creation of the Tab Group Submenu in the AppMenu
class TabGroupDynamicMenu {
 public:
  explicit TabGroupDynamicMenu(BrowserWindowInterface* bwi);
  TabGroupDynamicMenu(const TabGroupDynamicMenu&) = delete;
  TabGroupDynamicMenu& operator=(const TabGroupDynamicMenu&) = delete;
  ~TabGroupDynamicMenu();

  base::WeakPtr<TabGroupDynamicMenu> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BuildTabGroupsAction(actions::BaseAction* parent_item);

 private:
  void OnFaviconDataAvailable(
      base::WeakPtr<actions::ActionItem> action_item,
      const favicon_base::FaviconImageResult& image_result);

  void BuildTabGroupCommands(std::optional<tab_groups::SavedTabGroup> group,
                             actions::BaseAction* parent_item,
                             const base::Uuid& uuid,
                             Profile* profile);
  void BuildTabGroupData(std::optional<tab_groups::SavedTabGroup> group,
                         favicon::FaviconService* favicon_service,
                         actions::ActionItem* parent_item);

  void PerformTabGroupAction(tab_groups::TabGroupMenuAction::Type type,
                             BrowserWindowInterface* bwi,
                             actions::ActionItem* item,
                             actions::ActionInvocationContext context);

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<TabGroupDynamicMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_TAB_GROUP_DYNAMIC_MENU_H_
