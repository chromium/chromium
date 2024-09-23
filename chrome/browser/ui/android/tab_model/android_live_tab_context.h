// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

namespace base {
class Uuid;
}

namespace content {
class WebContents;
}

class TabModel;

// Implementation of LiveTabContext which is used by android.
class AndroidLiveTabContext : public sessions::LiveTabContext {
 public:
  explicit AndroidLiveTabContext(TabModel* tab_model);

  AndroidLiveTabContext(const AndroidLiveTabContext&) = delete;
  AndroidLiveTabContext& operator=(const AndroidLiveTabContext&) = delete;

  ~AndroidLiveTabContext() override {}

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
  const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const override;
  const std::optional<base::Uuid> GetSavedTabGroupIdForGroup(
      const tab_groups::TabGroupId& group) const override;
  bool IsTabPinned(int index) const override;
  void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  const gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  std::string GetWorkspace() const override;
  sessions::LiveTab* AddRestoredTab(
      const sessions::tab_restore::Tab& tab,
      int tab_index,
      bool select,
      sessions::tab_restore::Type original_session_type) override;
  sessions::LiveTab* ReplaceRestoredTab(
      const sessions::tab_restore::Tab&) override;
  void CloseTab() override;

  static LiveTabContext* FindContextForWebContents(
      const content::WebContents* contents);
  static sessions::LiveTabContext* FindContextWithID(SessionID desired_id);

 private:
  raw_ptr<TabModel> tab_model_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_
