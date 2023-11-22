// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_CHROME_DESKS_UTIL_H_
#define CHROME_BROWSER_UI_ASH_DESKS_CHROME_DESKS_UTIL_H_

#include <memory>

class TabGroupModel;

namespace app_restore {
struct TabGroupInfo;
}  // namespace app_restore

namespace chrome_desks_util {

// Name for app not available toast.
constexpr char kAppNotAvailableTemplateToastName[] =
    "AppNotAvailableTemplateToast";

// Given a TabGroupModel that contains at least a single TabGroup this method
// returns a vector that contains tab_groups::TabGroupInfo representations of
// the TabGroups contained within the model.
std::vector<tab_groups::TabGroupInfo> ConvertTabGroupsToTabGroupInfos(
    const TabGroupModel* group_model);

// Given a vector of TabGroupInfo this function attaches tab groups to the
// out_browser instance passed as the second parameter.
void AttachTabGroupsToBrowserInstance(
    const std::vector<tab_groups::TabGroupInfo>& tab_groups,
    Browser* browser);

// Sets tabs in `browser` to be pinned up to the `first_non_pinned_tab_index`.
void SetBrowserPinnedTabs(int32_t first_non_pinned_tab_index, Browser* browser);

}  // namespace chrome_desks_util

#endif  // CHROME_BROWSER_UI_ASH_DESKS_CHROME_DESKS_UTIL_H_
