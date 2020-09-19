// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_LIVE_TAB_CONTEXT_H_
#define COMPONENTS_SESSIONS_CORE_LIVE_TAB_CONTEXT_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Rect;
}

namespace sessions {

class LiveTab;
class SerializedNavigationEntry;
class PlatformSpecificTabData;

// An interface representing the context in which LiveTab instances exist in the
// embedder. As a concrete example, desktop Chrome has an implementation that
// is backed by an instance of the Browser class.
class SESSIONS_EXPORT LiveTabContext {
 public:
  // TODO(blundell): Rename.
  virtual void ShowBrowserWindow() = 0;
  virtual SessionID GetSessionID() const = 0;
  virtual int GetTabCount() const = 0;
  virtual int GetSelectedIndex() const = 0;
  virtual std::string GetAppName() const = 0;
  virtual std::string GetUserTitle() const = 0;
  virtual LiveTab* GetLiveTabAt(int index) const = 0;
  virtual LiveTab* GetActiveLiveTab() const = 0;
  virtual bool IsTabPinned(int index) const = 0;
  virtual base::Optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const = 0;
  // Should not be called for |group| unless GetTabGroupForTab() returned
  // |group|.
  virtual const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const = 0;
  // Update |group|'s metadata. Should only be called for |group| if a tab has
  // been restored in |group| via AddRestoredTab() or ReplaceRestoredTab().
  virtual void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) = 0;
  virtual const gfx::Rect GetRestoredBounds() const = 0;
  virtual ui::WindowShowState GetRestoredState() const = 0;
  virtual std::string GetWorkspace() const = 0;

  // Note: |tab_platform_data| may be null (e.g., if |from_last_session| is
  // true, as this data is not persisted, or if the platform does not provide
  // platform-specific data).
  virtual LiveTab* AddRestoredTab(
      const std::vector<SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      base::Optional<tab_groups::TabGroupId> group,
      const tab_groups::TabGroupVisualData& group_visual_data,
      bool select,
      bool pin,
      bool from_last_session,
      const PlatformSpecificTabData* tab_platform_data,
      const sessions::SerializedUserAgentOverride& user_agent_override) = 0;

  // Note: |tab_platform_data| may be null (e.g., if |from_last_session| is
  // true, as this data is not persisted, or if the platform does not provide
  // platform-specific data).
  virtual LiveTab* ReplaceRestoredTab(
      const std::vector<SerializedNavigationEntry>& navigations,
      base::Optional<tab_groups::TabGroupId> group,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      const PlatformSpecificTabData* tab_platform_data,
      const sessions::SerializedUserAgentOverride& user_agent_override) = 0;
  virtual void CloseTab() = 0;

 protected:
  virtual ~LiveTabContext() {}
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_LIVE_TAB_CONTEXT_H_
