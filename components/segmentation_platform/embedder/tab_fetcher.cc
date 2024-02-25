// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/tab_fetcher.h"

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"

namespace segmentation_platform {
namespace {

void FillTabsFromSessionsAfterTime(
    const std::vector<raw_ptr<const sync_sessions::SyncedSession,
                              VectorExperimental>> sessions,
    std::vector<TabFetcher::TabEntry>& tabs,
    base::Time tabs_loaded_after_timestamp) {
  for (const sync_sessions::SyncedSession* session : sessions) {
    for (const auto& session_and_window : session->windows) {
      const auto& window = session_and_window.second->wrapped_window;
      for (const auto& tab : window.tabs) {
        if (tab->timestamp >= tabs_loaded_after_timestamp) {
          tabs.emplace_back(tab->tab_id, session->GetSessionTag());
        }
      }
    }
  }
}

}  // namespace

TabFetcher::TabEntry::TabEntry(SessionID tab_id, const std::string& session_tag)
    : session_tag(session_tag), tab_id(tab_id), web_contents_data(nullptr) {}

TabFetcher::TabEntry::TabEntry(SessionID tab_id,
                               content::WebContents* webcontents,
                               TabAndroid* tab_android)
    : tab_id(tab_id),
      web_contents_data(webcontents),
      tab_android_data(tab_android) {}

TabFetcher::TabFetcher(sync_sessions::SessionSyncService* session_sync_service)
    : session_sync_service_(session_sync_service) {}

bool TabFetcher::FillAllRemoteTabs(std::vector<TabEntry>& tabs) {
  auto* open_ui_delegate = session_sync_service_->GetOpenTabsUIDelegate();
  if (!open_ui_delegate) {
    return false;
  }
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  open_ui_delegate->GetAllForeignSessions(&sessions);
  FillTabsFromSessionsAfterTime(sessions, tabs, base::Time());
  return true;
}

bool TabFetcher::FillAllRemoteTabsAfterTime(
    std::vector<TabEntry>& tabs,
    base::Time tabs_loaded_after_timestamp) {
  auto* open_ui_delegate = session_sync_service_->GetOpenTabsUIDelegate();
  if (!open_ui_delegate) {
    return false;
  }
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  open_ui_delegate->GetAllForeignSessions(&sessions);
  FillTabsFromSessionsAfterTime(sessions, tabs, tabs_loaded_after_timestamp);
  return true;
}

bool TabFetcher::FillAllLocalTabs(std::vector<TabEntry>& tabs) {
  if (FillAllLocalTabsFromTabModel(tabs)) {
    return true;
  }
  return FillAllLocalTabsFromSyncSessions(tabs);
}

TabFetcher::Tab TabFetcher::FindTab(const TabEntry& entry) {
  auto* open_ui_delegate = session_sync_service_->GetOpenTabsUIDelegate();
  if (!open_ui_delegate || entry.session_tag.empty()) {
    return FindLocalTab(entry);
  }
  const sessions::SessionTab* tab;
  open_ui_delegate->GetForeignTab(entry.session_tag, entry.tab_id, &tab);
  GURL url =
      tab->navigations.size() ? tab->navigations.back().virtual_url() : GURL();
  return Tab{.session_tab = tab,
             .tab_url = url,
             .time_since_modified = base::Time::Now() - tab->timestamp};
}

bool TabFetcher::FillAllLocalTabsFromTabModel(std::vector<TabEntry>& tabs) {
  NOTIMPLEMENTED();
  return false;
}

bool TabFetcher::FillAllLocalTabsFromSyncSessions(std::vector<TabEntry>& tabs) {
  const sync_sessions::SyncedSession* local_session = nullptr;
  auto* open_ui_delegate = session_sync_service_->GetOpenTabsUIDelegate();
  open_ui_delegate->GetLocalSession(&local_session);
  if (!local_session) {
    return false;
  }
  FillTabsFromSessionsAfterTime({local_session}, tabs, base::Time());
  return true;
}

TabFetcher::Tab TabFetcher::FindLocalTab(const TabEntry& entry) {
  NOTIMPLEMENTED();
  return Tab{};
}

size_t TabFetcher::GetRemoteTabsCountAfterTime(
    base::Time tabs_loaded_after_timestamp) {
  std::vector<TabFetcher::TabEntry> all_tabs;
  FillAllRemoteTabsAfterTime(all_tabs, tabs_loaded_after_timestamp);
  return all_tabs.size();
}

std::optional<base::Time> TabFetcher::GetLatestRemoteSessionModifiedTime() {
  auto* open_ui_delegate = session_sync_service_->GetOpenTabsUIDelegate();
  if (!open_ui_delegate) {
    return std::nullopt;
  }
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  open_ui_delegate->GetAllForeignSessions(&sessions);
  if (sessions.empty()) {
    return std::nullopt;
  }
  // Get latest session modified time.
  return sessions[0]->GetModifiedTime();
}

}  // namespace segmentation_platform
