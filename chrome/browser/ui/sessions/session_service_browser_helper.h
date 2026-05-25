// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SESSIONS_SESSION_SERVICE_BROWSER_HELPER_H_
#define CHROME_BROWSER_UI_SESSIONS_SESSION_SERVICE_BROWSER_HELPER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/sessions/core/session_id.h"

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
                              Profile* profile);
  ~SessionServiceBrowserHelper() override;

  SessionServiceBrowserHelper(const SessionServiceBrowserHelper&) = delete;
  SessionServiceBrowserHelper& operator=(const SessionServiceBrowserHelper&) =
      delete;

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

  const raw_ref<TabStripModel> tab_strip_model_;
  const SessionID session_id_;
  const BrowserWindowInterface::Type browser_type_;
  const raw_ref<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_SESSIONS_SESSION_SERVICE_BROWSER_HELPER_H_
