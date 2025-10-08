// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_action.h"

namespace tab_groups {

TabGroupMenuAction::TabGroupMenuAction(Type type,
                                       std::variant<base::Uuid, GURL> element)
    : type(type), element(element) {}
TabGroupMenuAction::TabGroupMenuAction(const TabGroupMenuAction& action) =
    default;
TabGroupMenuAction::~TabGroupMenuAction() = default;

}  // namespace tab_groups
