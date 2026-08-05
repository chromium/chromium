// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SESSIONS_SESSION_SERVICE_BROWSER_HELPER_H_
#define CHROME_BROWSER_UI_SESSIONS_SESSION_SERVICE_BROWSER_HELPER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/sessions/core/session_id.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/platform_session_manager.h"
#endif

class TabStripModel;
class Profile;
class SessionServiceBase;

namespace tabs {
class TabInterface;
}

namespace tab_groups {
class TabGroupId;
}

namespace split_tabs {
class SplitTabId;
}

// Helper class to sync tab and window state with SessionService and
// TabRestoreService to enable session restore. It observes TabStripModel
// and forwards relevant events.
class SessionServiceBrowserHelper : public TabStripModelObserver {
 public:
  SessionServiceBrowserHelper(TabStripModel* tab_strip_model,
                              SessionID session_id,
                              BrowserWindowInterface::Type browser_type,
                              Profile* profile,
                              const Browser::CreateParams* create_params);
  ~SessionServiceBrowserHelper() override;

  SessionServiceBrowserHelper(const SessionServiceBrowserHelper&) = delete;
  SessionServiceBrowserHelper& operator=(const SessionServiceBrowserHelper&) =
      delete;

#if BUILDFLAG(IS_OZONE)
  const std::optional<ui::PlatformSessionWindowData>& platform_session_data()
      const {
    return platform_session_data_;
  }
#endif

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;

 private:
  void SyncHistoryWithTabs(int index);
  void UpdateTabGroupSessionDataForTab(
      tabs::TabInterface* tab,
      std::optional<tab_groups::TabGroupId> group);
  void UpdateSplitTabSessionData(
      tabs::TabInterface* tab,
      std::optional<split_tabs::SplitTabId> split_id);
  void UpdateSplitTabSessionVisualData(const split_tabs::SplitTabId& split_id);

  SessionServiceBase* GetSessionService();
  SessionServiceBase* GetSessionServiceIfExisting();

#if BUILDFLAG(IS_OZONE)
  // If supported by the platform, this stores data related to the
  // windowing system level session. E.g: session and window IDs. See
  // ui/ozone/public/platform_session_manager.h for more details.
  std::optional<ui::PlatformSessionWindowData> platform_session_data_ =
      std::nullopt;
#endif

  const raw_ref<TabStripModel> tab_strip_model_;
  const SessionID session_id_;
  const BrowserWindowInterface::Type browser_type_;
  const raw_ref<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_SESSIONS_SESSION_SERVICE_BROWSER_HELPER_H_
