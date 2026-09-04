// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/chrome_desks_util.h"

#include "base/logging.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "components/tab_groups/tab_group_info.h"

namespace chrome_desks_util {

void AttachTabGroupsToBrowserInstance(
    const std::vector<tab_groups::TabGroupInfo>& tab_groups,
    ash::BrowserDelegate* browser) {
  const size_t tab_count = browser->GetWebContentsCount();
  for (const tab_groups::TabGroupInfo& tab_group : tab_groups) {
    if (tab_group.tab_range.IsValid() && !tab_group.tab_range.is_empty() &&
        !tab_group.tab_range.is_reversed() &&
        tab_group.tab_range.end() <= tab_count) {
      browser->CreateTabGroup(tab_group);
    } else {
      LOG(WARNING) << "Skipping tab group restoration: invalid range "
                   << tab_group.tab_range.ToString();
    }
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
