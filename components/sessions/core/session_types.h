// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SESSION_TYPES_H_
#define COMPONENTS_SESSIONS_CORE_SESSION_TYPES_H_

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/token.h"
#include "build/chromeos_buildflags.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/sessions_export.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/variations/variations_associated_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace sessions {

// SessionTab ----------------------------------------------------------------

// SessionTab corresponds to a NavigationController.
struct SESSIONS_EXPORT SessionTab {
  SessionTab();

  SessionTab(const SessionTab&) = delete;
  SessionTab& operator=(const SessionTab&) = delete;

  ~SessionTab();

  // Since the current_navigation_index can be larger than the index for number
  // of navigations in the current sessions (chrome://newtab is not stored), we
  // must perform bounds checking.
  // Returns a normalized bounds-checked navigation_index.
  int normalized_navigation_index() const {
    return std::max(0, std::min(current_navigation_index,
                                static_cast<int>(navigations.size() - 1)));
  }

  // Unique id of the window.
  SessionID window_id;

  // Unique id of the tab.
  SessionID tab_id;

  // Visual index of the tab within its window. There may be gaps in these
  // values.
  //
  // NOTE: this is really only useful for the SessionService during
  // restore, others can likely ignore this and use the order of the
  // tabs in SessionWindow.tabs.
  int tab_visual_index;

  // Identifies the index of the current navigation in navigations. For
  // example, if this is 2 it means the current navigation is navigations[2].
  //
  // NOTE: when the service is creating SessionTabs, initially this corresponds
  // to SerializedNavigationEntry.index, not the index in navigations. When done
  // creating though, this is set to the index in navigations.
  //
  // NOTE 2: this value can be larger than the size of |navigations|, due to
  // only valid url's being stored (ie chrome://newtab is not stored). Bounds
  // checking must be performed before indexing into |navigations|.
  int current_navigation_index;

  // The tab's group ID, if any.
  std::optional<tab_groups::TabGroupId> group;

  // True if the tab is pinned.
  bool pinned;

  // If non-empty, this tab is an app tab and this is the id of the extension.
  std::string extension_app_id;

  // If non-empty, this string is used as the user agent whenever the tab's
  // NavigationEntries need it overridden.
  SerializedUserAgentOverride user_agent_override;

  // Timestamp for when this tab was last modified.
  base::Time timestamp;

  // Timestamp for when this tab was last activated.
  // Corresponds to WebContents::GetLastActiveTime().
  base::Time last_active_time;

  std::vector<sessions::SerializedNavigationEntry> navigations;

  // For reassociating sessionStorage.
  std::string session_storage_persistent_id;

  // guid associated with the tab, may be empty.
  std::string guid;

  // Data associated with the tab by the embedder.
  std::map<std::string, std::string> data;

  // Extra data associated with the tab.
  std::map<std::string, std::string> extra_data;
};

// SessionTabGroup -----------------------------------------------------------

// Describes a tab group referenced by some SessionTab entry in its group
// field. By default, this is initialized with placeholder values that are
// visually obvious.
struct SESSIONS_EXPORT SessionTabGroup {
  explicit SessionTabGroup(const tab_groups::TabGroupId& id);

  SessionTabGroup(const SessionTabGroup&) = delete;
  SessionTabGroup& operator=(const SessionTabGroup&) = delete;

  ~SessionTabGroup();

  // Uniquely identifies this group. Initialized to zero and must be set be
  // user. Unlike SessionID this should be globally unique, even across
  // different sessions.
  tab_groups::TabGroupId id;

  tab_groups::TabGroupVisualData visual_data;

  // Used to notify the SavedTabGroupModel that this restore group was once
  // saved and should track any changes made on the group.
  std::optional<std::string> saved_guid;
};

// SessionWindow -------------------------------------------------------------

// Describes a saved window.
struct SESSIONS_EXPORT SessionWindow {
  SessionWindow();

  SessionWindow(const SessionWindow&) = delete;
  SessionWindow& operator=(const SessionWindow&) = delete;

  ~SessionWindow();

  // Possible window types which can be stored here. Note that these values will
  // be written out to disc via session commands.
  enum WindowType {
    TYPE_NORMAL = 0,
    TYPE_POPUP = 1,
    TYPE_APP = 2,
    TYPE_DEVTOOLS = 3,
    TYPE_APP_POPUP = 4,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    TYPE_CUSTOM_TAB = 5,
#endif
  };

  // Identifier of the window.
  SessionID window_id;

  // Bounds of the window.
  gfx::Rect bounds;

  // The workspace in which the window resides.
  std::string workspace;

  // Whether the window is visible on all workspaces or not.
  bool visible_on_all_workspaces;

  // Index of the selected tab in tabs; -1 if no tab is selected. After restore
  // this value is guaranteed to be a valid index into tabs.
  //
  // NOTE: when the service is creating SessionWindows, initially this
  // corresponds to SessionTab.tab_visual_index, not the index in
  // tabs. When done creating though, this is set to the index in
  // tabs.
  int selected_tab_index;

  // Type of the window. Note: This type is used to determine if the window gets
  // saved or not.
  WindowType type;

  // If true, the window is constrained.
  //
  // Currently SessionService prunes all constrained windows so that session
  // restore does not attempt to restore them.
  bool is_constrained;

  // Timestamp for when this window was last modified.
  base::Time timestamp;

  // The tabs, ordered by visual order.
  std::vector<std::unique_ptr<SessionTab>> tabs;

  // Tab groups in no particular order. For each group in |tab_groups|, there
  // should be at least one tab in |tabs| in the group.
  std::vector<std::unique_ptr<SessionTabGroup>> tab_groups;

  // Is the window maximized, minimized, or normal?
  ui::mojom::WindowShowState show_state;

  std::string app_name;

  // The user-configured title for this window, may be empty.
  std::string user_title;

  // Extra data associated with the window.
  std::map<std::string, std::string> extra_data;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SESSION_TYPES_H_
