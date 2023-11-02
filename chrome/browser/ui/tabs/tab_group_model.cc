// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_model.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_controller.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

TabGroupModel::TabGroupModel(TabGroupController* controller)
    : controller_(controller) {}

TabGroupModel::~TabGroupModel() {}

TabGroup* TabGroupModel::AddTabGroup(
    const tab_groups::TabGroupId& id,
    absl::optional<tab_groups::TabGroupVisualData> visual_data) {
  // The tab group must not already exist - replacing the old group without
  // first removing it would invalidate pointers to the old group and could
  // easily UAF.
  CHECK(!ContainsTabGroup(id));

  auto tab_group = std::make_unique<TabGroup>(
      controller_, id,
      visual_data.value_or(
          tab_groups::TabGroupVisualData(std::u16string(), GetNextColor())));
  groups_[id] = std::move(tab_group);

  return groups_[id].get();
}

bool TabGroupModel::ContainsTabGroup(const tab_groups::TabGroupId& id) const {
  return base::Contains(groups_, id);
}

TabGroup* TabGroupModel::GetTabGroup(const tab_groups::TabGroupId& id) const {
  DCHECK(ContainsTabGroup(id));
  return groups_.find(id)->second.get();
}

void TabGroupModel::RemoveTabGroup(const tab_groups::TabGroupId& id) {
  DCHECK(ContainsTabGroup(id));
  groups_.erase(id);
}

std::vector<tab_groups::TabGroupId> TabGroupModel::ListTabGroups() const {
  std::vector<tab_groups::TabGroupId> group_ids;
  group_ids.reserve(groups_.size());
  for (const auto& id_group_pair : groups_)
    group_ids.push_back(id_group_pair.first);
  return group_ids;
}

tab_groups::TabGroupColorId TabGroupModel::GetNextColor() const {
  // Count the number of times each available color is used.
  std::map<tab_groups::TabGroupColorId, int> color_usage_counts;
  for (const auto& id_color_pair : tab_groups::GetTabGroupColorLabelMap())
    color_usage_counts[id_color_pair.first] = 0;
  for (const auto& id_group_pair : groups_)
    color_usage_counts[id_group_pair.second->visual_data()->color()]++;

  // Find the next least-used color.
  tab_groups::TabGroupColorId next_color = color_usage_counts.begin()->first;
  int min_usage_count = color_usage_counts.begin()->second;
  for (const auto& color_usage_pair : color_usage_counts) {
    if (color_usage_pair.second < min_usage_count) {
      next_color = color_usage_pair.first;
      min_usage_count = color_usage_pair.second;
    }
  }
  return next_color;
}
