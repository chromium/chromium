// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_UTIL_H_

#include <optional>
#include <string>

#include "components/tab_groups/tab_group_id.h"

class Browser;
class Profile;
class TabGroupModel;

namespace ui {
class OSExchangeData;
}

namespace tab_strip_ui {

std::optional<tab_groups::TabGroupId> GetTabGroupIdFromString(
    TabGroupModel* tab_group_model,
    std::string group_id_string);

// Find the browser containing the group with ID |group_id_string| or nullptr if
// none. If the profile is not specified, find any browser containing the
// |group_id|.
Browser* GetBrowserWithGroupId(Profile* profile, std::string group_id_string);

void MoveTabAcrossWindows(
    Browser* source_browser,
    int from_index,
    Browser* target_browser,
    int to_index,
    std::optional<tab_groups::TabGroupId> to_group_id = std::nullopt);

// Returns whether |drop_data| is a tab drag originating from a WebUI
// tab strip.
bool IsDraggedTab(const ui::OSExchangeData& drop_data);

// Handles dropping tabs not destined for an existing tab strip.
// |new_browser| should be the newly created Browser with no tabs, and
// must have the same profile as the drag source. |drop_data| must have
// originated from a drag in a WebUI tab strip. If successful, the tabs
// reflected in |drop_data| will be moved from the source browser to
// |new_browser|.
bool DropTabsInNewBrowser(Browser* new_browser,
                          const ui::OSExchangeData& drop_data);

// Handles dropping tabs not destined for an existing tab strip.
// |new_browser| should be the newly created Browser with no tabs, and
// must have the same profile as the drag source. |tab_id| and/or
// |tab_group_id| must have originated from a drag in a WebUI tab strip.
// If successful, the tabs reflected in either |tab_id| or |tab_group_id|
// will be moved from the source browser to |new_browser|.
bool DropTabsInNewBrowser(Browser* new_browser,
                          const std::u16string& tab_id,
                          const std::u16string& tab_group_id);

// Helper that extracts tab_id and/or tab_group_id of a given |drop_data|.
bool ExtractTabData(const ui::OSExchangeData& drop_data,
                    std::u16string* tab_id,
                    std::u16string* tab_group_id);

}  // namespace tab_strip_ui

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_UTIL_H_
