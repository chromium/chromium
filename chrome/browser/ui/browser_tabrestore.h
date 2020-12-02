// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TABRESTORE_H_
#define CHROME_BROWSER_UI_BROWSER_TABRESTORE_H_

#include <string>
#include <vector>

#include "base/token.h"
#include "components/sessions/core/session_types.h"

class Browser;

namespace content {
class SessionStorageNamespace;
class WebContents;
}  // namespace content

namespace sessions {
class SerializedNavigationEntry;
struct SerializedUserAgentOverride;
}

namespace chrome {

// Add a tab with its session history restored from the SessionRestore and
// TabRestoreService systems. If select is true, the tab is selected.
// |tab_index| gives the index to insert the tab at. |selected_navigation| is
// the index of the SerializedNavigationEntry in |navigations| to select. If
// |extension_app_id| is non-empty the tab is an app tab and |extension_app_id|
// is the id of the extension. If |group| has a value, it specifies the
// ID corresponding to the tab's group. If |pin| is true and |tab_index|/ is
// the last pinned tab, then the newly created tab is pinned.
// |user_agent_override| contains the string being used as the user agent for
// all of the tab's navigations when the regular user agent is overridden. If
// |from_session_restore| is true, the restored tab is created by session
// restore. |last_active_time| is the value to use to indicate the last time the
// WebContents was made active, if this is left default initialized then the
// creation time will be used. Returns the WebContents of the restored tab.
content::WebContents* AddRestoredTab(
    Browser* browser,
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    base::Optional<tab_groups::TabGroupId> group,
    bool select,
    bool pin,
    base::TimeTicks last_active_time,
    content::SessionStorageNamespace* storage_namespace,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    bool from_session_restore);

// Replaces the state of the currently selected tab with the session
// history restored from the SessionRestore and TabRestoreService systems.
// Returns the WebContents of the restored tab.
content::WebContents* ReplaceRestoredTab(
    Browser* browser,
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int selected_navigation,
    const std::string& extension_app_id,
    content::SessionStorageNamespace* session_storage_namespace,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    bool from_session_restore);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_TABRESTORE_H_
