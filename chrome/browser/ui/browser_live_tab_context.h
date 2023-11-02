// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIVE_TAB_CONTEXT_H_
#define CHROME_BROWSER_UI_BROWSER_LIVE_TAB_CONTEXT_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
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

  BrowserLiveTabContext(const BrowserLiveTabContext&) = delete;
  BrowserLiveTabContext& operator=(const BrowserLiveTabContext&) = delete;

  ~BrowserLiveTabContext() override {}

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

  // see Browser::Create
  static sessions::LiveTabContext* Create(
      Profile* profile,
      sessions::SessionWindow::WindowType type,
      const std::string& app_name,
      const gfx::Rect& bounds,
      ui::WindowShowState show_state,
      const std::string& workspace,
      const std::string& user_title,
      const std::map<std::string, std::string>& extra_data);

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
  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIVE_TAB_CONTEXT_H_
