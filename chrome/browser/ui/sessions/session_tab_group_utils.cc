// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sessions/session_tab_group_utils.h"

#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace session_tab_group_utils {

bool IsPersistableTabGroup(const TabStripModel& tab_strip_model,
                           const tab_groups::TabGroupId& group_id) {
  const TabGroupModel* const group_model = tab_strip_model.group_model();
  return group_model && group_model->ContainsTabGroup(group_id) &&
         !tab_strip_model.IsEphemeralTabGroup(group_id);
}

std::optional<tab_groups::TabGroupId> GetPersistableTabGroupForTab(
    const TabStripModel& tab_strip_model,
    int index) {
  const std::optional<tab_groups::TabGroupId> group_id =
      tab_strip_model.GetTabGroupForTab(index);
  return (group_id.has_value() &&
          IsPersistableTabGroup(tab_strip_model, *group_id))
             ? group_id
             : std::nullopt;
}

std::optional<tab_groups::TabGroupId> GetPersistableFocusedGroup(
    const TabStripModel& tab_strip_model) {
  const std::optional<tab_groups::TabGroupId> group_id =
      tab_strip_model.GetFocusedGroup();
  return (group_id.has_value() &&
          IsPersistableTabGroup(tab_strip_model, *group_id))
             ? group_id
             : std::nullopt;
}

}  // namespace session_tab_group_utils
