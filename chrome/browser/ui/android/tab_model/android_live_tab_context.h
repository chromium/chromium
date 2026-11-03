// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

namespace base {
class Uuid;
}

namespace content {
class WebContents;
}

class Profile;
class TabModel;

// Tab restore extra-data key holding the closed tab's Android tab id. Written
// for tabs saved through AndroidLiveTabContextCloseWrapper, and for a single
// tab saved while it is still in its tab model; persisted with the
// TabRestoreService entry. Used by undo restore to map restored tabs back to
// their original positions; ignored by Recent Tabs restore. Only written when
// kUndoableTabClosureRework is enabled.
inline constexpr char kUndoOldTabIdKey[] = "android.undo.old_tab_id";

// Implementation of LiveTabContext which is used by android.
class AndroidLiveTabContext : public sessions::LiveTabContext {
 public:
  explicit AndroidLiveTabContext(TabModel* tab_model);

  AndroidLiveTabContext(const AndroidLiveTabContext&) = delete;
  AndroidLiveTabContext& operator=(const AndroidLiveTabContext&) = delete;

  ~AndroidLiveTabContext() override = default;

  // Overridden from LiveTabContext:
  void ShowBrowserWindow() override;
  SessionID GetSessionID() const override;
  sessions::SessionWindow::WindowType GetWindowType() const override;
  int GetTabCount() const override;
  int GetSelectedIndex() const override;
  std::string GetAppName() const override;
  std::string GetUserTitle() const override;
  sessions::LiveTab* GetLiveTabAt(int index) const override;
  sessions::LiveTab* GetActiveLiveTab() const override;
  std::map<std::string, std::string> GetExtraDataForTab(
      int index) const override;
  std::map<std::string, std::string> GetExtraDataForWindow() const override;
  std::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const override;
  std::optional<split_tabs::SplitTabId> GetSplitForTab(
      int index) const override;
  const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const override;
  const split_tabs::SplitTabVisualData* GetVisualDataForSplit(
      const split_tabs::SplitTabId& split_id) const override;
  const std::optional<base::Uuid> GetSavedTabGroupIdForGroup(
      const tab_groups::TabGroupId& group) const override;
  const std::optional<tab_groups::TabGroupId> GetGroupIdForSavedGroup(
      const base::Uuid& saved) const override;
  bool IsEphemeralTabGroup(const tab_groups::TabGroupId& group) const override;
  bool IsTabPinned(int index) const override;
  void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  const std::optional<tab_groups::TabGroupId> GetInitialFocusedTabGroup()
      const override;
  void SetFocusedTabGroup(const tab_groups::TabGroupId& group) override;
  const gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  std::string GetWorkspace() const override;
  sessions::LiveTab* AddRestoredTab(
      const sessions::tab_restore::Tab& tab,
      int tab_index,
      bool select,
      bool is_restoring_group_or_window,
      sessions::tab_restore::Type original_session_type) override;
  sessions::LiveTab* ReplaceRestoredTab(
      const sessions::tab_restore::Tab&) override;
  void ReconstructSplit(
      sessions::LiveTab* leading_tab,
      sessions::LiveTab* trailing_tab,
      split_tabs::SplitTabId split_id,
      const split_tabs::SplitTabVisualData& visual_data) override;
  void CloseTab() override;

  static LiveTabContext* FindContextForWebContents(
      const content::WebContents* contents);
  // Finds the LiveTabContext with `desired_id` for `profile`, falling back to
  // the first non-incognito LiveTabContext belonging to `profile` if not found.
  // `profile` must be non-null.
  static sessions::LiveTabContext* FindContextWithID(SessionID desired_id,
                                                     Profile* profile);

 protected:
  // Returns the tab model this context restores into.
  TabModel* tab_model() const { return tab_model_; }

  // Builds a WebContents for the restored tab, including restoring its
  // navigations and glic state. Shared by Recent Tabs and undo restore.
  static std::unique_ptr<content::WebContents> CreateRestoredWebContents(
      Profile* profile,
      const sessions::tab_restore::Tab& tab);

 private:
  raw_ptr<TabModel> tab_model_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_
