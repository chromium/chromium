// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_model.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/types/pass_key.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"

TabGroupModel::TabGroupModel() = default;

TabGroupModel::~TabGroupModel() = default;

void TabGroupModel::AddTabGroup(TabGroup* group, base::PassKey<TabStripModel>) {
  // The tab group must not already exist - replacing the old group without
  // first removing it would invalidate pointers to the old group and could
  // easily UAF.
  CHECK(!ContainsTabGroup(group->id()));
  group_ids_.emplace_back(group->id());
  groups_[group->id()] = group;
}

bool TabGroupModel::ContainsTabGroup(const tab_groups::TabGroupId& id) const {
  return base::Contains(groups_, id);
}

TabGroup* TabGroupModel::GetTabGroup(const tab_groups::TabGroupId& id) const {
  CHECK(ContainsTabGroup(id));
  return groups_.find(id)->second.get();
}

void TabGroupModel::RemoveTabGroup(const tab_groups::TabGroupId& id,
                                   base::PassKey<TabStripModel>) {
  CHECK(ContainsTabGroup(id));
  std::erase(group_ids_, id);
  groups_.erase(id);
}

std::vector<tab_groups::TabGroupId> TabGroupModel::ListTabGroups() const {
  return group_ids_;
}

tab_groups::TabGroupColorId TabGroupModel::GetNextColor(
    base::PassKey<TabStripModel>) const {
  std::vector<tab_groups::TabGroupColorId> used_colors;
  for (const auto& id_group_pair : groups_) {
    used_colors.push_back(id_group_pair.second->visual_data()->color());
  }
  return tab_groups::GetNextColor(used_colors);
}
