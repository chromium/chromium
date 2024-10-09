// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_LIVE_TAB_CONTEXT_H_
#define COMPONENTS_SESSIONS_CORE_LIVE_TAB_CONTEXT_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"

namespace base {
class Uuid;
}

namespace gfx {
class Rect;
}

namespace sessions {

class LiveTab;

// An interface representing the context in which LiveTab instances exist in the
// embedder. As a concrete example, desktop Chrome has an implementation that
// is backed by an instance of the Browser class.
class SESSIONS_EXPORT LiveTabContext {
 public:
  // TODO(blundell): Rename.
  virtual void ShowBrowserWindow() = 0;
  virtual SessionID GetSessionID() const = 0;
  virtual SessionWindow::WindowType GetWindowType() const = 0;
  virtual int GetTabCount() const = 0;
  virtual int GetSelectedIndex() const = 0;
  virtual std::string GetAppName() const = 0;
  virtual std::string GetUserTitle() const = 0;
  virtual LiveTab* GetLiveTabAt(int index) const = 0;
  virtual LiveTab* GetActiveLiveTab() const = 0;
  virtual std::map<std::string, std::string> GetExtraDataForTab(
      int index) const = 0;
  virtual std::map<std::string, std::string> GetExtraDataForWindow() const = 0;
  virtual std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const = 0;
  // Should not be called for |group| unless GetTabGroupForTab() returned
  // |group|.
  virtual const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const = 0;
  virtual const std::optional<base::Uuid> GetSavedTabGroupIdForGroup(
      const tab_groups::TabGroupId& group) const = 0;
  virtual bool IsTabPinned(int index) const = 0;
  // Update |group|'s metadata. Should only be called for |group| if a tab has
  // been restored in |group| via AddRestoredTab() or ReplaceRestoredTab().
  virtual void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) = 0;
  virtual const gfx::Rect GetRestoredBounds() const = 0;
  virtual ui::mojom::WindowShowState GetRestoredState() const = 0;
  virtual std::string GetWorkspace() const = 0;

  // Note: |tab.platform_data| may be null (e.g., if restoring from last session
  // as this data is not persisted, or if the platform does not provide
  // platform-specific data).
  // |tab.id| is the tab's unique SessionID. Only present if a historical tab
  // has been created by TabRestoreService.
  // |original_session_type| indicates the type of session entry the tab
  // belongs to.
  virtual LiveTab* AddRestoredTab(const tab_restore::Tab& tab,
                                  int tab_index,
                                  bool select,
                                  tab_restore::Type original_session_type) = 0;

  // Note: |tab.platform_data| may be null (e.g., if restoring from last session
  // as this data is not persisted, or if the platform does not provide
  // platform-specific data).
  virtual LiveTab* ReplaceRestoredTab(const tab_restore::Tab& tab) = 0;
  virtual void CloseTab() = 0;

 protected:
  virtual ~LiveTabContext() = default;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_LIVE_TAB_CONTEXT_H_
