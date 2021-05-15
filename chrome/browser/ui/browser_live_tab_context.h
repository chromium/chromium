// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIVE_TAB_CONTEXT_H_
#define CHROME_BROWSER_UI_BROWSER_LIVE_TAB_CONTEXT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/ui_base_types.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

// Implementation of LiveTabContext which uses an instance of
// Browser in order to fulfil its duties.
class BrowserLiveTabContext : public sessions::LiveTabContext {
 public:
  explicit BrowserLiveTabContext(Browser* browser) : browser_(browser) {}
  ~BrowserLiveTabContext() override {}

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
  absl::optional<tab_groups::TabGroupId> GetTabGroupForTab(
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
      absl::optional<tab_groups::TabGroupId> group,
      const tab_groups::TabGroupVisualData& group_visual_data,
      bool select,
      bool pin,
      const sessions::PlatformSpecificTabData* storage_namespace,
      const sessions::SerializedUserAgentOverride& user_agent_override,
      const SessionID* tab_id) override;
  sessions::LiveTab* ReplaceRestoredTab(
      const std::vector<sessions::SerializedNavigationEntry>& navigations,
      absl::optional<tab_groups::TabGroupId> group,
      int selected_navigation,
      const std::string& extension_app_id,
      const sessions::PlatformSpecificTabData* tab_platform_data,
      const sessions::SerializedUserAgentOverride& user_agent_override)
      override;
  void CloseTab() override;

  // see Browser::Create
  static sessions::LiveTabContext* Create(Profile* profile,
                                          const std::string& app_name,
                                          const gfx::Rect& bounds,
                                          ui::WindowShowState show_state,
                                          const std::string& workspace,
                                          const std::string& user_title);

  // see browser::FindBrowserForWebContents
  static sessions::LiveTabContext* FindContextForWebContents(
      const content::WebContents* contents);

  // see chrome::FindBrowserWithID
  // Returns the LiveTabContext of the Browser with |desired_id| if
  // such a Browser exists.
  static sessions::LiveTabContext* FindContextWithID(SessionID desired_id);

  // see chrome::FindBrowserWithGroup
  // Returns the LiveTabContext of the Browser containing the group with ID
  // |group| if such a Browser exists within the given |profile|.
  static sessions::LiveTabContext* FindContextWithGroup(
      tab_groups::TabGroupId group,
      Profile* profile);

 private:
  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserLiveTabContext);
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIVE_TAB_CONTEXT_H_
