// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SESSIONS_SESSION_TAB_GROUP_UTILS_H_
#define CHROME_BROWSER_UI_SESSIONS_SESSION_TAB_GROUP_UTILS_H_

#include <optional>

#include "components/tab_groups/tab_group_id.h"

class TabStripModel;

// Helpers that decide which tab groups may be written to session storage
// (SessionService / TabRestoreService). Ephemeral (non-group focus) tab groups
// only exist to back focus mode and must never be persisted; they become
// persistable once promoted via `TabStripModel::PromoteEphemeralTabGroup()`.
// Every session writer should go through these helpers rather than checking
// `TabStripModel::IsEphemeralTabGroup()` by hand so the rule lives in one
// place.
namespace session_tab_group_utils {

// Returns whether `group_id` exists in `tab_strip_model` and may be persisted.
bool IsPersistableTabGroup(const TabStripModel& tab_strip_model,
                           const tab_groups::TabGroupId& group_id);

// Returns the group of the tab at `index`, or nullopt if the tab is not
// grouped or its group must not be persisted.
std::optional<tab_groups::TabGroupId> GetPersistableTabGroupForTab(
    const TabStripModel& tab_strip_model,
    int index);

// Returns the focused group, or nullopt if no group is focused or the focused
// group must not be persisted.
std::optional<tab_groups::TabGroupId> GetPersistableFocusedGroup(
    const TabStripModel& tab_strip_model);

}  // namespace session_tab_group_utils

#endif  // CHROME_BROWSER_UI_SESSIONS_SESSION_TAB_GROUP_UTILS_H_
