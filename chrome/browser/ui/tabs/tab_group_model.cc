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
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_controller.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

TabGroupModel::TabGroupModel(TabGroupController* controller)
    : controller_(controller) {}

TabGroupModel::~TabGroupModel() = default;

TabGroup* TabGroupModel::AddTabGroup(
    const tab_groups::TabGroupId& id,
    std::optional<tab_groups::TabGroupVisualData> visual_data) {
  // The tab group must not already exist - replacing the old group without
  // first removing it would invalidate pointers to the old group and could
  // easily UAF.
  CHECK(!ContainsTabGroup(id));

  auto tab_group = std::make_unique<TabGroup>(
      controller_, id,
      visual_data.value_or(
          tab_groups::TabGroupVisualData(std::u16string(), GetNextColor())));
  if (groups_.find(id) == groups_.end()) {
    group_ids_.emplace_back(id);
  }
  groups_[id] = std::move(tab_group);
  return groups_[id].get();
}

bool TabGroupModel::ContainsTabGroup(const tab_groups::TabGroupId& id) const {
  return base::Contains(groups_, id);
}

TabGroup* TabGroupModel::GetTabGroup(const tab_groups::TabGroupId& id) const {
  CHECK(ContainsTabGroup(id), base::NotFatalUntil::M127);
  return groups_.find(id)->second.get();
}

void TabGroupModel::RemoveTabGroup(const tab_groups::TabGroupId& id) {
  CHECK(ContainsTabGroup(id));
  group_ids_.erase(base::ranges::remove(group_ids_, id));
  groups_.erase(id);
}

std::vector<tab_groups::TabGroupId> TabGroupModel::ListTabGroups() const {
  return group_ids_;
}

tab_groups::TabGroupColorId TabGroupModel::GetNextColor() const {
  std::vector<tab_groups::TabGroupColorId> used_colors;
  for (const auto& id_group_pair : groups_) {
    used_colors.push_back(id_group_pair.second->visual_data()->color());
  }
  return tab_groups::GetNextColor(used_colors);
}
