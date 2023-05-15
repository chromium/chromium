// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_info.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace {

const std::vector<int> ConvertRangeToTabGroupIndices(const gfx::Range& range) {
  std::vector<int> indices;

  for (uint32_t index = range.start(); index < range.end(); ++index) {
    indices.push_back(static_cast<int>(index));
  }

  return indices;
}

}  // namespace

namespace chrome_desks_util {

std::vector<tab_groups::TabGroupInfo> ConvertTabGroupsToTabGroupInfos(
    const TabGroupModel* group_model) {
  DCHECK(group_model);
  const std::vector<tab_groups::TabGroupId>& listed_group_ids =
      group_model->ListTabGroups();

  std::vector<tab_groups::TabGroupInfo> tab_groups;
  for (const tab_groups::TabGroupId& group_id : listed_group_ids) {
    const TabGroup* tab_group = group_model->GetTabGroup(group_id);
    tab_groups.emplace_back(
        gfx::Range(tab_group->ListTabs()),
        tab_groups::TabGroupVisualData(*(tab_group->visual_data())));
  }

  return tab_groups;
}

void AttachTabGroupsToBrowserInstance(
    const std::vector<tab_groups::TabGroupInfo>& tab_groups,
    Browser* browser) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  for (const tab_groups::TabGroupInfo& tab_group : tab_groups) {
    tab_groups::TabGroupId new_group_id = tab_strip_model->AddToNewGroup(
        ConvertRangeToTabGroupIndices(tab_group.tab_range));
    tab_strip_model->group_model()
        ->GetTabGroup(new_group_id)
        ->SetVisualData(tab_group.visual_data);
  }
}

void SetBrowserPinnedTabs(int32_t first_non_pinned_tab_index,
                          Browser* browser) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  DCHECK(first_non_pinned_tab_index <= tab_strip_model->count());
  for (int32_t tab_index = 0; tab_index < first_non_pinned_tab_index;
       tab_index++) {
    tab_strip_model->SetTabPinned(tab_index, /*pinned=*/true);
  }
}

}  // namespace chrome_desks_util
