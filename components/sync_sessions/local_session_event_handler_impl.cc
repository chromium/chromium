// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/local_session_event_handler_impl.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace sync_sessions {
namespace {

using sessions::SerializedNavigationEntry;

// The maximum number of navigations in each direction we care to sync.
const int kMaxSyncNavigationCount = 6;

bool IsSessionRestoreInProgress(SyncSessionsClient* sessions_client) {
  DCHECK(sessions_client);
  SyncedWindowDelegatesGetter* synced_window_getter =
      sessions_client->GetSyncedWindowDelegatesGetter();
  SyncedWindowDelegatesGetter::SyncedWindowDelegateMap windows =
      synced_window_getter->GetSyncedWindowDelegates();
  for (const auto& window_iter_pair : windows) {
    if (window_iter_pair.second->IsSessionRestoreInProgress()) {
      return true;
    }
  }
  return false;
}

bool IsWindowSyncable(const SyncedWindowDelegate& window_delegate) {
  return window_delegate.ShouldSync() && window_delegate.GetTabCount() &&
         window_delegate.HasWindow();
}

// On Android, it's possible to not have any tabbed windows when only custom
// tabs are currently open. This means that there is tab data that will be
// restored later, but we cannot access it.
bool ScanForTabbedWindow(SyncedWindowDelegatesGetter* delegates_getter) {
  for (const auto& window_iter_pair :
       delegates_getter->GetSyncedWindowDelegates()) {
    const SyncedWindowDelegate* window_delegate = window_iter_pair.second;
    if (window_delegate->IsTypeNormal() && IsWindowSyncable(*window_delegate)) {
      return true;
    }
  }
  return false;
}

}  // namespace

LocalSessionEventHandlerImpl::WriteBatch::WriteBatch() = default;

LocalSessionEventHandlerImpl::WriteBatch::~WriteBatch() = default;

LocalSessionEventHandlerImpl::Delegate::~Delegate() = default;

LocalSessionEventHandlerImpl::LocalSessionEventHandlerImpl(
    Delegate* delegate,
    SyncSessionsClient* sessions_client,
    SyncedSessionTracker* session_tracker)
    : delegate_(delegate),
      sessions_client_(sessions_client),
      session_tracker_(session_tracker) {
  DCHECK(delegate);
  DCHECK(sessions_client);
  DCHECK(session_tracker);

  current_session_tag_ = session_tracker_->GetLocalSessionTag();
  DCHECK(!current_session_tag_.empty());

  if (!IsSessionRestoreInProgress(sessions_client)) {
    OnSessionRestoreComplete();
  }
}

LocalSessionEventHandlerImpl::~LocalSessionEventHandlerImpl() {}

void LocalSessionEventHandlerImpl::OnSessionRestoreComplete() {
  std::unique_ptr<WriteBatch> batch = delegate_->CreateLocalSessionWriteBatch();
  // The initial state of the tracker may contain tabs that are unmmapped but
  // haven't been marked as free yet.
  CleanupLocalTabs(batch.get());
  AssociateWindows(RELOAD_TABS, batch.get());
  batch->Commit();
}

sync_pb::SessionTab
LocalSessionEventHandlerImpl::GetTabSpecificsFromDelegateForTest(
    const SyncedTabDelegate& tab_delegate) const {
  return GetTabSpecificsFromDelegate(tab_delegate);
}

void LocalSessionEventHandlerImpl::CleanupLocalTabs(WriteBatch* batch) {
  std::set<int> deleted_tab_node_ids =
      session_tracker_->CleanupLocalTabs(base::BindRepeating(
          &Delegate::IsTabNodeUnsynced, base::Unretained(delegate_)));

  for (int tab_node_id : deleted_tab_node_ids) {
    batch->Delete(tab_node_id);
  }
}

void LocalSessionEventHandlerImpl::AssociateWindows(ReloadTabsOption option,
                                                    WriteBatch* batch) {
  DCHECK(!IsSessionRestoreInProgress(sessions_client_));

  const bool has_tabbed_window =
      ScanForTabbedWindow(sessions_client_->GetSyncedWindowDelegatesGetter());

  // Note that |current_session| is a pointer owned by |session_tracker_|.
  // |session_tracker_| will continue to update |current_session| under
  // the hood so care must be taken accessing it. In particular, invoking
  // ResetSessionTracking(..) will invalidate all the tab data within
  // the session, hence why copies of the SyncedSession must be made ahead of
  // time.
  SyncedSession* current_session =
      session_tracker_->GetSession(current_session_tag_);

  SyncedWindowDelegatesGetter::SyncedWindowDelegateMap windows =
      sessions_client_->GetSyncedWindowDelegatesGetter()
          ->GetSyncedWindowDelegates();

  // Without native data, we need be careful not to obliterate any old
  // information, while at the same time handling updated tab ids. See
  // https://crbug.com/639009 for more info.
  if (has_tabbed_window) {
    // Just reset the session tracking. No need to worry about the previous
    // session; the current tabbed windows are now the source of truth.
    session_tracker_->ResetSessionTracking(current_session_tag_);
    current_session->modified_time = base::Time::Now();
  } else {
    DVLOG(1) << "Found no tabbed windows. Reloading "
             << current_session->windows.size()
             << " windows from previous session.";
  }

  for (auto& window_iter_pair : windows) {
    const SyncedWindowDelegate* window_delegate = window_iter_pair.second;
    if (option == RELOAD_TABS) {
      UMA_HISTOGRAM_COUNTS_1M("Sync.SessionTabs",
                              window_delegate->GetTabCount());
    }

    // Make sure the window has tabs and a viewable window. The viewable
    // window check is necessary because, for example, when a browser is
    // closed the destructor is not necessarily run immediately. This means
    // its possible for us to get a handle to a browser that is about to be
    // removed. If the tab count is 0 or the window is null, the browser is
    // about to be deleted, so we ignore it.
    if (!IsWindowSyncable(*window_delegate)) {
      continue;
    }

    SessionID window_id = window_delegate->GetSessionId();
    DVLOG(1) << "Associating window " << window_id.id() << " with "
             << window_delegate->GetTabCount() << " tabs.";

    bool found_tabs = false;
    for (int j = 0; j < window_delegate->GetTabCount(); ++j) {
      SessionID tab_id = window_delegate->GetTabIdAt(j);
      SyncedTabDelegate* synced_tab = window_delegate->GetTabAt(j);

      // IsWindowSyncable(), via ShouldSync(), guarantees that tabs are not
      // null.
      DCHECK(synced_tab);

      // If for some reason the tab ID is invalid, skip it.
      if (!tab_id.is_valid()) {
        continue;
      }

      // Placeholder tabs are those without WebContents, either because they
      // were never loaded into memory or they were evicted from memory
      // (typically only on Android devices). They only have a window ID and a
      // tab ID,  and we can use the latter to properly reassociate the tab with
      // the entity that was backing it. The window ID could have changed, but
      // noone really cares, because the window/tab hierarchy is constructed
      // from the header entity (which has up-to-date IDs). Hence, in order to
      // avoid unnecessary traffic, we avoid updating the entity.
      if (!synced_tab->IsPlaceholderTab() && RELOAD_TABS == option) {
        AssociateTab(synced_tab, batch);
      }

      // If the tab was syncable, it would have been added to the tracker either
      // by the above AssociateTab call or by the OnLocalTabModified method
      // invoking AssociateTab directly. Therefore, we can key whether this
      // window has valid tabs based on the tab's presence in the tracker.
      const sessions::SessionTab* tab =
          session_tracker_->LookupSessionTab(current_session_tag_, tab_id);
      if (tab) {
        found_tabs = true;

        // Update this window's representation in the synced session tracker.
        // This is a no-op if called multiple times.
        session_tracker_->PutWindowInSession(current_session_tag_, window_id);

        // Put the tab in the window (must happen after the window is added
        // to the session).
        session_tracker_->PutTabInWindow(current_session_tag_, window_id,
                                         tab_id);
      }
    }
    if (found_tabs) {
      SyncedSessionWindow* synced_session_window =
          current_session->windows[window_id].get();
      if (window_delegate->IsTypeNormal()) {
        synced_session_window->window_type =
            sync_pb::SessionWindow_BrowserType_TYPE_TABBED;
      } else if (window_delegate->IsTypePopup()) {
        synced_session_window->window_type =
            sync_pb::SessionWindow_BrowserType_TYPE_POPUP;
      } else {
        // This is a custom tab within an app. These will not be restored on
        // startup if not present.
        synced_session_window->window_type =
            sync_pb::SessionWindow_BrowserType_TYPE_CUSTOM_TAB;
      }
    }
  }

  CleanupLocalTabs(batch);

  // Always update the header.  Sync takes care of dropping this update
  // if the entity specifics are identical (i.e windows, client name did
  // not change).
  auto specifics = std::make_unique<sync_pb::SessionSpecifics>();
  specifics->set_session_tag(current_session_tag_);
  current_session->ToSessionHeaderProto().Swap(specifics->mutable_header());
  batch->Put(std::move(specifics));
}

void LocalSessionEventHandlerImpl::AssociateTab(
    SyncedTabDelegate* const tab_delegate,
    WriteBatch* batch) {
  DCHECK(!tab_delegate->IsPlaceholderTab());

  if (tab_delegate->IsBeingDestroyed()) {
    // Do nothing else. By not proactively adding the tab to the session, it
    // will be removed if necessary during subsequent cleanup.
    return;
  }

  if (!tab_delegate->ShouldSync(sessions_client_)) {
    return;
  }

  SessionID tab_id = tab_delegate->GetSessionId();
  int tab_node_id =
      session_tracker_->LookupTabNodeFromTabId(current_session_tag_, tab_id);

  if (tab_node_id == TabNodePool::kInvalidTabNodeID) {
    // Allocate a new (or reused) sync node for this tab.
    tab_node_id = session_tracker_->AssociateLocalTabWithFreeTabNode(tab_id);
    DCHECK_NE(TabNodePool::kInvalidTabNodeID, tab_node_id)
        << "https://crbug.com/639009";
  }

  DVLOG(1) << "Syncing tab " << tab_id << " from window "
           << tab_delegate->GetWindowId() << " using tab node " << tab_node_id;

  // Get the previously synced url.
  sessions::SessionTab* session_tab =
      session_tracker_->GetTab(current_session_tag_, tab_id);
  int old_index = session_tab->normalized_navigation_index();
  GURL old_url;
  if (session_tab->navigations.size() > static_cast<size_t>(old_index))
    old_url = session_tab->navigations[old_index].virtual_url();

  // Produce the specifics.
  auto specifics = std::make_unique<sync_pb::SessionSpecifics>();
  specifics->set_session_tag(current_session_tag_);
  specifics->set_tab_node_id(tab_node_id);
  GetTabSpecificsFromDelegate(*tab_delegate).Swap(specifics->mutable_tab());
  WriteTasksIntoSpecifics(specifics->mutable_tab(), tab_delegate);

  // Update the tracker's session representation. Timestamp will be overwriten,
  // so we set a null time first to prevent the update from being ignored, if
  // the local clock is skewed.
  session_tab->timestamp = base::Time();
  UpdateTrackerWithSpecifics(*specifics, base::Time::Now(), session_tracker_);
  DCHECK(!session_tab->timestamp.is_null());

  // Write to the sync model itself.
  batch->Put(std::move(specifics));

  int current_index = tab_delegate->GetCurrentEntryIndex();
  const GURL new_url = tab_delegate->GetVirtualURLAtIndex(current_index);
  if (current_index >= 0 && new_url != old_url) {
    delegate_->OnFaviconVisited(
        new_url, tab_delegate->GetFaviconURLAtIndex(current_index));
  }
}

void LocalSessionEventHandlerImpl::WriteTasksIntoSpecifics(
    sync_pb::SessionTab* tab_specifics,
    SyncedTabDelegate* tab_delegate) {
  for (int i = 0; i < tab_specifics->navigation_size(); i++) {
    // Excluding blocked navigations, which are appended at tail.
    if (tab_specifics->navigation(i).blocked_state() ==
        sync_pb::TabNavigation::STATE_BLOCKED) {
      break;
    }
    int64_t task_id = tab_delegate->GetTaskIdForNavigationId(
        tab_specifics->navigation(i).unique_id());
    int64_t parent_task_id = tab_delegate->GetParentTaskIdForNavigationId(
        tab_specifics->navigation(i).unique_id());
    int64_t root_task_id = tab_delegate->GetRootTaskIdForNavigationId(
        tab_specifics->navigation(i).unique_id());

    tab_specifics->mutable_navigation(i)->set_task_id(task_id);
    tab_specifics->mutable_navigation(i)->add_ancestor_task_id(root_task_id);
    tab_specifics->mutable_navigation(i)->add_ancestor_task_id(parent_task_id);
  }
}

void LocalSessionEventHandlerImpl::OnLocalTabModified(
    SyncedTabDelegate* modified_tab) {
  DCHECK(!current_session_tag_.empty());

  // Defers updates if session restore is in progress.
  if (IsSessionRestoreInProgress(sessions_client_)) {
    return;
  }

  sessions::SerializedNavigationEntry current;
  modified_tab->GetSerializedNavigationAtIndex(
      modified_tab->GetCurrentEntryIndex(), &current);
  delegate_->TrackLocalNavigationId(current.timestamp(), current.unique_id());

  std::unique_ptr<WriteBatch> batch = delegate_->CreateLocalSessionWriteBatch();
  AssociateTab(modified_tab, batch.get());
  // Note, we always associate windows because it's possible a tab became
  // "interesting" by going to a valid URL, in which case it needs to be added
  // to the window's tab information. Similarly, if a tab became
  // "uninteresting", we remove it from the window's tab information.
  AssociateWindows(DONT_RELOAD_TABS, batch.get());
  batch->Commit();
}

void LocalSessionEventHandlerImpl::OnFaviconsChanged(
    const std::set<GURL>& page_urls,
    const GURL& /* icon_url */) {
  for (const GURL& page_url : page_urls) {
    if (page_url.is_valid()) {
      delegate_->OnPageFaviconUpdated(page_url);
    }
  }
}

sync_pb::SessionTab LocalSessionEventHandlerImpl::GetTabSpecificsFromDelegate(
    const SyncedTabDelegate& tab_delegate) const {
  sync_pb::SessionTab specifics;
  specifics.set_window_id(tab_delegate.GetWindowId().id());
  specifics.set_tab_id(tab_delegate.GetSessionId().id());
  specifics.set_tab_visual_index(0);
  // Use -1 to indicate that the index hasn't been set properly yet.
  specifics.set_current_navigation_index(-1);
  const SyncedWindowDelegate* window_delegate =
      sessions_client_->GetSyncedWindowDelegatesGetter()->FindById(
          tab_delegate.GetWindowId());
  specifics.set_pinned(
      window_delegate ? window_delegate->IsTabPinned(&tab_delegate) : false);
  specifics.set_extension_app_id(tab_delegate.GetExtensionAppId());
  const int current_index = tab_delegate.GetCurrentEntryIndex();
  const int min_index = std::max(0, current_index - kMaxSyncNavigationCount);
  const int max_index = std::min(current_index + kMaxSyncNavigationCount,
                                 tab_delegate.GetEntryCount());
  bool is_supervised = tab_delegate.ProfileIsSupervised();

  for (int i = min_index; i < max_index; ++i) {
    if (!tab_delegate.GetVirtualURLAtIndex(i).is_valid()) {
      continue;
    }
    sessions::SerializedNavigationEntry serialized_entry;
    tab_delegate.GetSerializedNavigationAtIndex(i, &serialized_entry);

    // Set current_navigation_index to the index in navigations.
    if (i == current_index)
      specifics.set_current_navigation_index(specifics.navigation_size());

    sync_pb::TabNavigation* navigation = specifics.add_navigation();
    SessionNavigationToSyncData(serialized_entry).Swap(navigation);

    const std::string page_language = tab_delegate.GetPageLanguageAtIndex(i);
    if (!page_language.empty())
      navigation->set_page_language(page_language);

    if (is_supervised) {
      navigation->set_blocked_state(
          sync_pb::TabNavigation_BlockedState_STATE_ALLOWED);
    }
  }

  // If the current navigation is invalid, set the index to the end of the
  // navigation array.
  if (specifics.current_navigation_index() < 0) {
    specifics.set_current_navigation_index(specifics.navigation_size() - 1);
  }

  if (is_supervised) {
    const std::vector<std::unique_ptr<const SerializedNavigationEntry>>&
        blocked_navigations = *tab_delegate.GetBlockedNavigations();
    for (size_t i = 0; i < blocked_navigations.size(); ++i) {
      sync_pb::TabNavigation* navigation = specifics.add_navigation();
      SessionNavigationToSyncData(*blocked_navigations[i]).Swap(navigation);
      navigation->set_blocked_state(
          sync_pb::TabNavigation_BlockedState_STATE_BLOCKED);
      // TODO(bauerb): Add categories
    }
  }

  return specifics;
}

}  // namespace sync_sessions
