// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_UTILS_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_UTILS_H_

#include <variant>

#include "base/uuid.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "url/gurl.h"

namespace tab_groups {

// The action users can perform on a saved tab group's submenu.
struct TabGroupMenuAction {
  enum class Type {
    DEFAULT = -1,
    OPEN_IN_BROWSER,
    OPEN_OR_MOVE_TO_NEW_WINDOW,
    PIN_OR_UNPIN_GROUP,
    DELETE_GROUP,
    LEAVE_GROUP,
    OPEN_URL,
  };

  TabGroupMenuAction(Type type, std::variant<base::Uuid, GURL> element);
  TabGroupMenuAction(const TabGroupMenuAction&);
  ~TabGroupMenuAction();

  Type type = Type::DEFAULT;

  // The action needs either a UUID (e.g. Open group in new window) or a URL
  // (e.g. Open tab) to perform.
  std::variant<base::Uuid, GURL> element;
};

enum class TabGroupMenuContext {
  SAVED_TAB_GROUP_BUTTON_CONTEXT_MENU,
  SAVED_TAB_GROUP_EVERYTHING_MENU,
  APP_MENU,
  MAC_SYSTEM_MENU
};

class TabGroupMenuUtils {
 public:
  static std::u16string GetMenuTextForGroup(
      const tab_groups::SavedTabGroup& group);

  static std::u16string GetMenuTextForTab(
      const tab_groups::SavedTabGroupTab& tab);

  // Returns sorted saved tab groups with the most recently created as the
  // first, filtering out empty groups.
  static std::vector<base::Uuid> GetGroupsForDisplaySortedByCreationTime(
      TabGroupSyncService* wrapper_service);
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_UTILS_H_
