// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PROPERTIES_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PROPERTIES_H_

#include <optional>

#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "ui/actions/action_id.h"

namespace page_actions {

// Defines the static properties that a page action can have. The page action in
// mainly configured using the ActionItem. But the ActionItem is global.
// Therefore, for some properties, they should be scoped to page actions only.
struct PageActionProperties {
  const char* histogram_name = nullptr;
  bool is_ephemeral = false;
  PageActionIconType type;
};

// Get the properties associated with the given action id. The provided action
// id is expected to exist in the configs array.
std::optional<PageActionProperties> GetPageActionProperties(
    actions::ActionId page_action_id);

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PROPERTIES_H_
