// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_model.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/types/pass_key.h"
#include "chrome/browser/ui/ui_features.h"
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
  group_ids_by_activity_.emplace_front(group->id());
}

bool TabGroupModel::ContainsTabGroup(const tab_groups::TabGroupId& id) const {
  return groups_.contains(id);
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
  group_ids_by_activity_.remove(id);
}

std::vector<tab_groups::TabGroupId> TabGroupModel::ListTabGroups() const {
  return group_ids_;
}

std::optional<tab_groups::TabGroupId> TabGroupModel::GetMostRecentTabGroupId()
    const {
  if (group_ids_by_activity_.empty()) {
    return std::nullopt;
  } else {
    return {group_ids_by_activity_.front()};
  }
}

void TabGroupModel::OnTabGroupActivated(const tab_groups::TabGroupId& id,
                                        base::PassKey<TabStripModel>) {
  // This group was activated so we have to  push it to the front
  CHECK(ContainsTabGroup(id));

  if (group_ids_by_activity_.front() == id) {
    return;
  }

  group_ids_by_activity_.remove(id);
  group_ids_by_activity_.emplace_front(id);
}

tab_groups::TabGroupColorId TabGroupModel::GetNextColor(
    base::PassKey<TabStripModel>) const {
  // Count the number of times each color is used.
  std::map<tab_groups::TabGroupColorId, int> color_usage_counts;
  for (const auto& id_color_pair : tab_groups::GetTabGroupColorLabelMap()) {
    color_usage_counts[id_color_pair.first] = 0;
  }

  for (const auto& id_group_pair : groups_) {
    ++color_usage_counts[id_group_pair.second->visual_data()->color()];
  }

  // Compute the minimum number of usages across all colors.
  int min_usage_count = color_usage_counts.begin()->second;
  for (const auto& color_usage_pair : color_usage_counts) {
    min_usage_count = std::min(min_usage_count, color_usage_pair.second);
  }

  // Get the first color in an order with minimum usage.
  for (tab_groups::TabGroupColorId color_id : GetColorOrdering()) {
    if (color_usage_counts[color_id] == min_usage_count) {
      return color_id;
    }
  }
  NOTREACHED();
}

// static
std::vector<tab_groups::TabGroupColorId> TabGroupModel::GetColorOrdering() {
  if (base::FeatureList::IsEnabled(features::kTabGroupColorRefresh)) {
    return {tab_groups::TabGroupColorId::kBlue,
            tab_groups::TabGroupColorId::kPurple,
            tab_groups::TabGroupColorId::kPink,
            tab_groups::TabGroupColorId::kRed,
            tab_groups::TabGroupColorId::kOrange,
            tab_groups::TabGroupColorId::kYellow,
            tab_groups::TabGroupColorId::kGreen,
            tab_groups::TabGroupColorId::kCyan,
            tab_groups::TabGroupColorId::kGrey};
  } else {
    // Use the ordering defined by values of each member in the enum, in
    // ascending order.
    const tab_groups::ColorLabelMap& color_map =
        tab_groups::GetTabGroupColorLabelMap();
    std::vector<tab_groups::TabGroupColorId> color_ids;

    for (const auto& pair : color_map) {
      color_ids.push_back(pair.first);
    }
    return color_ids;
  }
}
