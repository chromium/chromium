// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/open_tabs_ui_delegate_impl.h"

#include <algorithm>

#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_session_tracker.h"

namespace sync_sessions {
namespace {

// Comparator function for use with std::sort that will sort tabs by
// descending timestamp (i.e., most recent first).
bool TabsRecencyComparator(const sessions::SessionTab* t1,
                           const sessions::SessionTab* t2) {
  return t1->timestamp > t2->timestamp;
}

// Comparator function for use with std::sort that will sort sessions by
// descending modified_time (i.e., most recent first).
bool SessionsRecencyComparator(const SyncedSession* s1,
                               const SyncedSession* s2) {
  return s1->modified_time > s2->modified_time;
}

}  // namespace

OpenTabsUIDelegateImpl::OpenTabsUIDelegateImpl(
    const SyncSessionsClient* sessions_client,
    const SyncedSessionTracker* session_tracker,
    const FaviconCache* favicon_cache,
    const DeleteForeignSessionCallback& delete_foreign_session_cb)
    : sessions_client_(sessions_client),
      session_tracker_(session_tracker),
      favicon_cache_(favicon_cache),
      delete_foreign_session_cb_(delete_foreign_session_cb) {}

OpenTabsUIDelegateImpl::~OpenTabsUIDelegateImpl() {}

favicon_base::FaviconRawBitmapResult
OpenTabsUIDelegateImpl::GetSyncedFaviconForPageURL(const GURL& page_url) const {
  return favicon_cache_->GetSyncedFaviconForPageURL(page_url);
}

bool OpenTabsUIDelegateImpl::GetAllForeignSessions(
    std::vector<const SyncedSession*>* sessions) {
  *sessions = session_tracker_->LookupAllForeignSessions(
      SyncedSessionTracker::PRESENTABLE);
  std::sort(sessions->begin(), sessions->end(), SessionsRecencyComparator);
  return !sessions->empty();
}

bool OpenTabsUIDelegateImpl::GetForeignSession(
    const std::string& tag,
    std::vector<const sessions::SessionWindow*>* windows) {
  return session_tracker_->LookupSessionWindows(tag, windows);
}

bool OpenTabsUIDelegateImpl::GetForeignTab(const std::string& tag,
                                           const SessionID tab_id,
                                           const sessions::SessionTab** tab) {
  *tab = session_tracker_->LookupSessionTab(tag, tab_id);
  return *tab != nullptr;
}

bool OpenTabsUIDelegateImpl::GetForeignSessionTabs(
    const std::string& tag,
    std::vector<const sessions::SessionTab*>* tabs) {
  std::vector<const sessions::SessionWindow*> windows;
  if (!session_tracker_->LookupSessionWindows(tag, &windows))
    return false;

  // Prune those tabs that are not syncable or are NewTabPage, then sort them
  // from most recent to least recent, independent of which window the tabs were
  // from.
  for (size_t j = 0; j < windows.size(); ++j) {
    const sessions::SessionWindow* window = windows[j];
    for (size_t t = 0; t < window->tabs.size(); ++t) {
      sessions::SessionTab* const tab = window->tabs[t].get();
      if (tab->navigations.empty())
        continue;
      const sessions::SerializedNavigationEntry& current_navigation =
          tab->navigations.at(tab->normalized_navigation_index());
      if (!sessions_client_->ShouldSyncURL(current_navigation.virtual_url()))
        continue;
      tabs->push_back(tab);
    }
  }
  std::stable_sort(tabs->begin(), tabs->end(), TabsRecencyComparator);
  return true;
}

void OpenTabsUIDelegateImpl::DeleteForeignSession(const std::string& tag) {
  delete_foreign_session_cb_.Run(tag);
}

bool OpenTabsUIDelegateImpl::GetLocalSession(
    const SyncedSession** local_session) {
  *local_session = session_tracker_->LookupLocalSession();
  return *local_session != nullptr;
}

GURL OpenTabsUIDelegateImpl::GetIconUrlForPageUrl(const GURL& page_url) {
  return favicon_cache_->GetIconUrlForPageUrl(page_url);
}

}  // namespace sync_sessions
