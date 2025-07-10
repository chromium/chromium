// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/chrome_desks_util.h"

#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_info.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"

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
    ash::BrowserDelegate* browser) {
  for (const tab_groups::TabGroupInfo& tab_group : tab_groups) {
    browser->CreateTabGroup(tab_group);
  }
}

void SetBrowserPinnedTabs(int32_t first_non_pinned_tab_index,
                          ash::BrowserDelegate* browser) {
  for (int32_t tab_index = 0; tab_index < first_non_pinned_tab_index;
       tab_index++) {
    browser->PinTab(tab_index);
  }
}

}  // namespace chrome_desks_util
