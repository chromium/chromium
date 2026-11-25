// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_UTILS_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_UTILS_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/uuid.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "ui/base/class_property.h"
#include "url/gurl.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(base::Uuid*)

namespace tab_groups {

// GUID of the saved tab group that a tab group menu action item targets.
extern const ui::ClassProperty<base::Uuid*>* const kSavedTabGroupGuidKey;

// The action users can perform on a saved tab group's submenu.
struct TabGroupMenuAction {
  // These values are persisted to logs. Entries should not be renumbered and
  // number values should never be reused.
  // LINT.IfChange(TabGroupMenuAction)
  enum class Type {
    DEFAULT = -1,
    OPEN_IN_BROWSER = 0,
    CLOSE_GROUP = 1,
    OPEN_OR_MOVE_TO_NEW_WINDOW = 2,
    FOCUS_OR_UNFOCUS_GROUP = 3,
    PIN_OR_UNPIN_GROUP = 4,
    DELETE_GROUP = 5,
    LEAVE_GROUP = 6,
    OPEN_URL = 7,
    kMaxValue = OPEN_URL,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:TabGroupMenuAction)

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
  ORGANIZER_PANEL,
};

class TabGroupMenuUtils {
 public:
  static std::u16string GetMenuTextForGroup(std::u16string_view title,
                                            size_t tab_count);

  static std::u16string GetMenuTextForTab(std::u16string_view title,
                                          const GURL& url);

  // Returns sorted saved tab groups with the most recently created as the
  // first, filtering out empty groups.
  static std::vector<base::Uuid> GetGroupsForDisplaySortedByCreationTime(
      TabGroupSyncService* wrapper_service);
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_UTILS_H_
