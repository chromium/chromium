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
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

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
  absl::optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const override;
  const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const override;
  bool IsTabPinned(int index) const override;
  void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) override;
  const gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  std::string GetWorkspace() const override;
  sessions::LiveTab* AddRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      absl::optional<tab_groups::TabGroupId> group,
      const tab_groups::TabGroupVisualData& group_visual_data,
      bool select,
      bool pin,
      const sessions::PlatformSpecificTabData* storage_namespace,
      const sessions::SerializedUserAgentOverride& user_agent_override,
      const std::map<std::string, std::string>& extra_data,
      const SessionID* tab_id) override;
  sessions::LiveTab* ReplaceRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      absl::optional<tab_groups::TabGroupId> group,
      int selected_navigation,
      const std::string& extension_app_id,
      const sessions::PlatformSpecificTabData* tab_platform_data,
      const sessions::SerializedUserAgentOverride& user_agent_override,
      const std::map<std::string, std::string>& extra_data) override;
  void CloseTab() override;

  static LiveTabContext* FindContextForWebContents(
      const content::WebContents* contents);
  static sessions::LiveTabContext* FindContextWithID(SessionID desired_id);

 private:
  raw_ptr<TabModel> tab_model_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_
