// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_TYPES_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_TYPES_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/serialized_user_agent_override.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"

namespace sessions::tab_restore {

// Interface used to allow the test to provide a custom time.
class SESSIONS_EXPORT TimeFactory {
 public:
  virtual ~TimeFactory();
  virtual base::Time TimeNow() = 0;
};

// A class that is used to associate platform-specific data with
// TabRestoreTab. See LiveTab::GetPlatformSpecificTabData().
class SESSIONS_EXPORT PlatformSpecificTabData {
 public:
  virtual ~PlatformSpecificTabData();
};

// The type of entry.
enum Type {
  TAB,
  GROUP,
  WINDOW,
};

// Represent a previously open object (a memento) that is stored in memory
// allowing us to identify and restore specific entries using their `id` to a
// previous state.
struct SESSIONS_EXPORT Entry {
  Entry(const Entry&) = delete;
  Entry& operator=(const Entry&) = delete;

  virtual ~Entry();

  // Unique id for this entry. The id is guaranteed to be unique for a
  // session.
  SessionID id;

  // The original id of the entry when it was saved.
  SessionID original_id;

  // The type of the entry.
  const Type type;

  // The time when the window, tab, or group was closed. Not always set - can
  // be nullptr or 0 in cases where a timestamp isn't available at entry
  // creation.
  base::Time timestamp;

  // Used for storing arbitrary key/value pairs.
  std::map<std::string, std::string> extra_data;

  // Estimates memory usage. By default returns 0.
  virtual size_t EstimateMemoryUsage() const;

 protected:
  explicit Entry(Type type);
};

// Represents a previously open tab.
// If you add a new field that can allocate memory also add
// it to the EstimatedMemoryUsage() implementation.
struct SESSIONS_EXPORT Tab : public Entry {
  Tab();
  ~Tab() override;

  // Entry:
  size_t EstimateMemoryUsage() const override;

  // Since the current_navigation_index can be larger than the index for
  // number of navigations in the current sessions (chrome://newtab is not
  // stored), we must perform bounds checking. Returns a normalized
  // bounds-checked navigation_index.
  int normalized_navigation_index() const {
    return std::max(0, std::min(current_navigation_index,
                                static_cast<int>(navigations.size() - 1)));
  }

  // The navigations.
  std::vector<sessions::SerializedNavigationEntry> navigations;

  // Index of the selected navigation in navigations.
  int current_navigation_index = -1;

  // The ID of the browser to which this tab belonged, so it can be restored
  // there. May be 0 (an invalid SessionID) when restoring an entire session.
  SessionID::id_type browser_id = 0;

  // Index within the tab strip. May be -1 for an unknown index.
  int tabstrip_index = -1;

  // True if the tab was pinned.
  bool pinned = false;

  // If non-empty gives the id of the extension for the tab.
  std::string extension_app_id;

  // The associated client data.
  std::unique_ptr<PlatformSpecificTabData> platform_data;

  // The user agent override used for the tab's navigations (if applicable).
  sessions::SerializedUserAgentOverride user_agent_override;

  // The group the tab belonged to, if any.
  std::optional<tab_groups::TabGroupId> group;

  // The saved group id the tab belong to, if any.
  std::optional<base::Uuid> saved_group_id = std::nullopt;

  // The group metadata for the tab, if any.
  std::optional<tab_groups::TabGroupVisualData> group_visual_data;
};

// Represents a previously open group.
// If you add a new field that can allocate memory also add
// it to the EstimatedMemoryUsage() implementation.
struct SESSIONS_EXPORT Group : public Entry {
  Group();
  ~Group() override;

  // Entry:
  size_t EstimateMemoryUsage() const override;

  // Creates a new Group object using the group metadata from a tab. CHECK if
  // `tab` has a group value.
  static std::unique_ptr<Group> FromTab(const Tab& tab);

  // The tabs that comprised the group, in order.
  std::vector<std::unique_ptr<Tab>> tabs;

  // Group metadata.
  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::CreateEmpty();
  tab_groups::TabGroupVisualData visual_data;

  // The saved group id of this group, if any.
  std::optional<base::Uuid> saved_group_id = std::nullopt;

  // The ID of the browser to which this group belonged, so it can be restored
  // there.
  SessionID::id_type browser_id = 0;
};

// Represents a previously open window.
// If you add a new field that can allocate memory, please also add
// it to the EstimatedMemoryUsage() implementation.
struct SESSIONS_EXPORT Window : public Entry {
  Window();
  ~Window() override;

  // Entry:
  size_t EstimateMemoryUsage() const override;

  // Type of window.
  sessions::SessionWindow::WindowType type;

  // The tabs that comprised the window, in order.
  std::vector<std::unique_ptr<Tab>> tabs;

  // The tab groups in the window. These are only used to query properties about
  // a group such as visual data, collapsed state, and saved state. As such,
  // groups in this structure should NOT contain any tabs.
  std::map<tab_groups::TabGroupId, std::unique_ptr<Group>> tab_groups;

  // Index of the selected tab.
  int selected_tab_index = -1;

  // If an application window, the name of the app.
  std::string app_name;

  // User-set title of the window, if there is one.
  std::string user_title;

  // Where and how the window is displayed.
  gfx::Rect bounds;
  ui::mojom::WindowShowState show_state;
  std::string workspace;
};

}  // namespace sessions::tab_restore

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_TYPES_H_
