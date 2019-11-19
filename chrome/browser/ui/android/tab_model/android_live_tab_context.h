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
  sessions::LiveTab* GetLiveTabAt(int index) const override;
  sessions::LiveTab* GetActiveLiveTab() const override;
  bool IsTabPinned(int index) const override;
  base::Optional<base::Token> GetTabGroupForTab(int index) const override;
  TabGroupMetadata GetTabGroupMetadata(base::Token group) const override;
  const gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  std::string GetWorkspace() const override;
  sessions::LiveTab* AddRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      int tab_index,
      int selected_navigation,
      const std::string& extension_app_id,
      base::Optional<base::Token> group,
      bool select,
      bool pin,
      bool from_last_session,
      const sessions::PlatformSpecificTabData* storage_namespace,
      const std::string& user_agent_override) override;
  sessions::LiveTab* ReplaceRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      base::Optional<base::Token> group,
      int selected_navigation,
      bool from_last_session,
      const std::string& extension_app_id,
      const sessions::PlatformSpecificTabData* tab_platform_data,
      const std::string& user_agent_override) override;
  void CloseTab() override;
  void SetTabGroupMetadata(base::Token group,
                           TabGroupMetadata group_metadata) override;

  static LiveTabContext* FindContextForWebContents(
      const content::WebContents* contents);
  static sessions::LiveTabContext* FindContextWithID(SessionID desired_id);

 private:
  TabModel* tab_model_;
  DISALLOW_COPY_AND_ASSIGN(AndroidLiveTabContext);
};


#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_ANDROID_LIVE_TAB_CONTEXT_H_
