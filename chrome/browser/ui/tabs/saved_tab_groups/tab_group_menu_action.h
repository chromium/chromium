// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_ACTION_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_ACTION_H_

#include <variant>

#include "base/uuid.h"
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

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_MENU_ACTION_H_
