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

  // see Browser::Create
  static sessions::LiveTabContext* Create(Profile* profile,
                                          const std::string& app_name,
                                          const gfx::Rect& bounds,
                                          ui::WindowShowState show_state,
                                          const std::string& workspace);

  // see browser::FindBrowserForWebContents
  static sessions::LiveTabContext* FindContextForWebContents(
      const content::WebContents* contents);

  // see chrome::FindBrowserWithID
  // Returns the LiveTabContext of the Browser with |desired_id| if
  // such a Browser exists.
  static sessions::LiveTabContext* FindContextWithID(SessionID desired_id);

 private:
  Browser* const browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserLiveTabContext);
};

#endif  // CHROME_BROWSER_UI_BROWSER_LIVE_TAB_CONTEXT_H_
