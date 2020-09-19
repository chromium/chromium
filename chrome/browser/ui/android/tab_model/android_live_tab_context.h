// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
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
  ~AndroidLiveTabContext() override {}

  // Overridden from LiveTabContext:
  void ShowBrowserWindow() override;
  SessionID GetSessionID() const override;
  int GetTabCount() const override;
  int GetSelectedIndex() const override;
  std::string GetAppName() const override;
  std::string GetUserTitle() const override;
  sessions::LiveTab* GetLiveTabAt(int index) const override;
  sessions::LiveTab* GetActiveLiveTab() const override;
  bool IsTabPinned(int index) const override;
  base::Optional<tab_groups::TabGroupId> GetTabGroupForTab(
      int index) const override;
  const tab_groups::TabGroupVisualData* GetVisualDataForGroup(
      const tab_groups::TabGroupId& group) const override;
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
      base::Optional<tab_groups::TabGroupId> group,
      const tab_groups::TabGroupVisualData& group_visual_data,
      bool select,
      bool pin,
      bool from_last_session,
      const sessions::PlatformSpecificTabData* storage_namespace,
      const sessions::SerializedUserAgentOverride& user_agent_override)
      override;
  sessions::LiveTab* ReplaceRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      base::Optional<tab_groups::TabGroupId> group,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      const sessions::PlatformSpecificTabData* tab_platform_data,
      const sessions::SerializedUserAgentOverride& user_agent_override)
      override;
  void CloseTab() override;

  static LiveTabContext* FindContextForWebContents(
      const content::WebContents* contents);
  static sessions::LiveTabContext* FindContextWithID(SessionID desired_id);

 private:
  TabModel* tab_model_;
  DISALLOW_COPY_AND_ASSIGN(AndroidLiveTabContext);
};


#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_
