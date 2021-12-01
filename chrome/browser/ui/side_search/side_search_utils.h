// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_UTILS_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_UTILS_H_

#include <map>
#include <tuple>

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace side_search {

// Adds side search state data to a tab's state restore data if applicable.
void MaybeAddSideSearchTabRestoreData(
    content::WebContents* web_contents,
    std::map<std::string, std::string>& extra_data);

// Add side search state data for a window's state restore data if applicable.
void MaybeAddSideSearchWindowRestoreData(
    bool toggled_open,
    std::map<std::string, std::string>& extra_data);

void MaybeRestoreSideSearchWindowState(
    SideSearchTabContentsHelper::Delegate* delegate,
    const std::map<std::string, std::string>& extra_data);

void SetSideSearchStateFromRestoreData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data);

}  // namespace side_search

bool IsSideSearchEnabled(const Profile* profile);

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_UTILS_H_
