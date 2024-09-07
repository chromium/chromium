// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/local_session_event_handler_impl.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_session_tracker.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace sync_sessions {
namespace {

using sessions::SerializedNavigationEntry;

// Enumeration of possible results when placeholder tabs are attempted to be
// resynced. Used in UMA metrics. Do not re-order or delete these entries; they
// are used in a UMA histogram. Please edit SyncPlaceholderTabResyncResult in
// enums.xml if a value is added.
// LINT.IfChange(SyncPlaceholderTabResyncResult)
enum PlaceholderTabResyncResultHistogramValue {
  PLACEHOLDER_TAB_FOUND = 0,
  PLACEHOLDER_TAB_RESYNCED = 1,
  PLACEHOLDER_TAB_NOT_SYNCED = 2,
  PLACEHOLDER_TAB_RESYNC_FAILED = 3,

  kMaxValue = PLACEHOLDER_TAB_RESYNC_FAILED
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncPlaceholderTabResyncResult)

// The maximum number of navigations in each direction we care to sync.
const int kMaxSyncNavigationCount = 6;

bool IsSessionRestoreInProgress(SyncSessionsClient* sessions_client) {
  DCHECK(sessions_client);
  SyncedWindowDelegatesGetter* synced_window_getter =
      sessions_client->GetSyncedWindowDelegatesGetter();
  SyncedWindowDelegatesGetter::SyncedWindowDelegateMap window_delegates =
      synced_window_getter->GetSyncedWindowDelegates();
  for (const auto& [window_id, window_delegate] : window_delegates) {
    if (window_delegate->IsSessionRestoreInProgress()) {
      return true;
    }
  }
  return false;
}

bool IsWindowSyncable(const SyncedWindowDelegate& window_delegate) {
  return window_delegate.ShouldSync() && window_delegate.HasWindow();
}

// On Android, it's possible to not have any tabbed windows when only custom
// tabs are currently open. This means that there is tab data that will be
// restored later, but we cannot access it.
bool ScanForTabbedWindow(SyncedWindowDelegatesGetter* delegates_getter) {
  for (const auto& [window_id, window_delegate] :
       delegates_getter->GetSyncedWindowDelegates()) {
    if (window_delegate->IsTypeNormal() && IsWindowSyncable(*window_delegate)) {
      return true;
    }
  }
  return false;
}

sync_pb::SyncEnums_BrowserType BrowserTypeFromWindowDelegate(
    const SyncedWindowDelegate& delegate) {
  if (delegate.IsTypeNormal()) {
    return sync_pb::SyncEnums_BrowserType_TYPE_TABBED;
  }

  if (delegate.IsTypePopup()) {
    return sync_pb::SyncEnums_BrowserType_TYPE_POPUP;
  }

  // This is a custom tab within an app. These will not be restored on
  // startup if not present.
  return sync_pb::SyncEnums_BrowserType_TYPE_CUSTOM_TAB;
}

#if BUILDFLAG(IS_ANDROID)
void RecordPlaceholderTabResyncResult(
    PlaceholderTabResyncResultHistogramValue result_value) {
  base::UmaHistogramEnumeration("Sync.PlaceholderTabResyncResult",
                                result_value);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

LocalSessionEventHandlerImpl::WriteBatch::WriteBatch() = default;

LocalSessionEventHandlerImpl::WriteBatch::~WriteBatch() = default;

LocalSessionEventHandlerImpl::Delegate::~Delegate() = default;

LocalSessionEventHandlerImpl::LocalSessionEventHandlerImpl(
    Delegate* delegate,
    SyncSessionsClient* sessions_client,
    SyncedSessionTracker* session_tracker,
    bool is_new_session)
    : delegate_(delegate),
      sessions_client_(sessions_client),
      session_tracker_(session_tracker) {
  DCHECK(delegate);
  DCHECK(sessions_client);
  DCHECK(session_tracker);

  current_session_tag_ = session_tracker_->GetLocalSessionTag();
  DCHECK(!current_session_tag_.empty());

  if (is_new_session) {
    session_tracker_->SetLocalSessionStartTime(base::Time::Now());
  }

  if (!IsSessionRestoreInProgress(sessions_client)) {
    OnSessionRestoreComplete();
  }
}

LocalSessionEventHandlerImpl::~LocalSessionEventHandlerImpl() = default;

void LocalSessionEventHandlerImpl::OnSessionRestoreComplete() {
  std::unique_ptr<WriteBatch> batch = delegate_->CreateLocalSessionWriteBatch();
  // The initial state of the tracker may contain tabs that are unmmapped but
  // haven't been marked as free yet.
  CleanupLocalTabs(batch.get());
  AssociateWindows(RELOAD_TABS, batch.get(), /*is_session_restore=*/true);
  batch->Commit();
}

sync_pb::SessionTab
LocalSessionEventHandlerImpl::GetTabSpecificsFromDelegateForTest(
    SyncedTabDelegate& tab_delegate) const {
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
                                                    WriteBatch* batch,
                                                    bool is_session_restore) {
  DCHECK(!IsSessionRestoreInProgress(sessions_client_));
  base::ElapsedTimer timer;

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

  SyncedWindowDelegatesGetter::SyncedWindowDelegateMap window_delegates =
      sessions_client_->GetSyncedWindowDelegatesGetter()
          ->GetSyncedWindowDelegates();

  // Without native data, we need be careful not to obliterate any old
  // information, while at the same time handling updated tab ids. See
  // https://crbug.com/639009 for more info.
  if (has_tabbed_window) {
    // Just reset the session tracking. No need to worry about the previous
    // session; the current tabbed windows are now the source of truth.
    session_tracker_->ResetSessionTracking(current_session_tag_);
    current_session->SetModifiedTime(base::Time::Now());
  } else {
    DVLOG(1) << "Found no tabbed windows. Reloading "
             << current_session->windows.size()
             << " windows from previous session.";
  }

  for (auto& [window_id, window_delegate] : window_delegates) {
    // Make sure the window is viewable and is not about to be closed. The
    // viewable window check is necessary because, for example, when a browser
    // is closed the destructor is not necessarily run immediately. This means
    // its possible for us to get a handle to a browser that is about to be
    // removed. If the window is null, the browser is about to be deleted, so we
    // ignore it. There is no check for having open tabs anymore. This is needed
    // to handle a case when the last tab is closed (on Andorid it doesn't mean
    // that the window is about to be removed). Instead, there is a check if the
    // window is about to be closed. If the window is last for the profile, the
    // latest state will be kept.
    if (!IsWindowSyncable(*window_delegate)) {
      continue;
    }

    const int tab_count_in_window = window_delegate->GetTabCount();
    DCHECK_EQ(window_id, window_delegate->GetSessionId());
    DVLOG(1) << "Associating window " << window_id.id() << " with "
             << tab_count_in_window << " tabs.";

    bool found_tabs = false;
    for (int j = 0; j < tab_count_in_window; ++j) {
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

#if BUILDFLAG(IS_ANDROID)
      // Metrics recording will only occur if AssociateWindows is called through
      // a session restore, denoted by is_session_restore.
      if (synced_tab->IsPlaceholderTab()) {
        if (tab && is_session_restore) {
          RecordPlaceholderTabResyncResult(PLACEHOLDER_TAB_FOUND);
        } else if (!tab) {
          // The placeholder tab doesn't have a tracked counterpart. This is
          // possible, for example, if the tab was created as a placeholder tab.
          bool was_tab_resynced = AssociatePlaceholderTab(
              synced_tab->ReadPlaceholderTabSnapshotIfItShouldSync(
                  sessions_client_),
              batch);

          if (was_tab_resynced) {
            // If the tab was presumed to have resynced successfully, perform
            // another lookup.
            tab = session_tracker_->LookupSessionTab(current_session_tag_,
                                                     tab_id);

            if (is_session_restore) {
              RecordPlaceholderTabResyncResult(
                  tab ? PLACEHOLDER_TAB_RESYNCED
                      : PLACEHOLDER_TAB_RESYNC_FAILED);
            }
          } else if (is_session_restore) {
            RecordPlaceholderTabResyncResult(PLACEHOLDER_TAB_RESYNC_FAILED);
          }
        } else if (is_session_restore) {
          // This metric logic path will likely record no tab data as long as
          // the RestoreSyncedPlaceholderTabs flag is enabled. If it is
          // disabled, this path will record all placeholder tabs that the
          // flag-guarded logic would have attempted to target.
          RecordPlaceholderTabResyncResult(PLACEHOLDER_TAB_NOT_SYNCED);
        }
      }
#endif  // BUILDFLAG(IS_ANDROID)

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
      synced_session_window->window_type =
          BrowserTypeFromWindowDelegate(*window_delegate);
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

  if (is_session_restore) {
    UmaHistogramMediumTimes("Sync.AssociateWindowsTime.OnSessionRestore",
                            timer.Elapsed());
  } else {
    UmaHistogramMediumTimes("Sync.AssociateWindowsTime.OnTabModification",
                            timer.Elapsed());
  }
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
  if (session_tab->navigations.size() > static_cast<size_t>(old_index)) {
    old_url = session_tab->navigations[old_index].virtual_url();
  }

  // Produce the specifics.
  auto specifics = std::make_unique<sync_pb::SessionSpecifics>();
  specifics->set_session_tag(current_session_tag_);
  specifics->set_tab_node_id(tab_node_id);
  GetTabSpecificsFromDelegate(*tab_delegate).Swap(specifics->mutable_tab());

  // Update the tracker's session representation. Timestamp will be overwriten,
  // so we set a null time first to prevent the update from being ignored, if
  // the local clock is skewed.
  session_tab->timestamp = base::Time();
  UpdateTrackerWithSpecifics(*specifics, base::Time::Now(), session_tracker_);
  DCHECK(!session_tab->timestamp.is_null());

  // Write to the sync model itself.
  batch->Put(std::move(specifics));
}

void LocalSessionEventHandlerImpl::OnLocalTabModified(
    SyncedTabDelegate* modified_tab) {
  DCHECK(!current_session_tag_.empty());

  // Defers updates if session restore is in progress.
  if (IsSessionRestoreInProgress(sessions_client_)) {
    return;
  }

  // Don't track empty tabs.
  if (modified_tab->GetEntryCount() != 0) {
    sessions::SerializedNavigationEntry current;
    modified_tab->GetSerializedNavigationAtIndex(
        modified_tab->GetCurrentEntryIndex(), &current);
    delegate_->TrackLocalNavigationId(current.timestamp(), current.unique_id());
  }

  std::unique_ptr<WriteBatch> batch = delegate_->CreateLocalSessionWriteBatch();
  AssociateTab(modified_tab, batch.get());
  // Note, we always associate windows because it's possible a tab became
  // "interesting" by going to a valid URL, in which case it needs to be added
  // to the window's tab information. Similarly, if a tab became
  // "uninteresting", we remove it from the window's tab information.
  AssociateWindows(DONT_RELOAD_TABS, batch.get(), /*is_session_restore=*/false);
  batch->Commit();
}

sync_pb::SessionTab LocalSessionEventHandlerImpl::GetTabSpecificsFromDelegate(
    SyncedTabDelegate& tab_delegate) const {
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
  specifics.set_last_active_time_unix_epoch_millis(
      (tab_delegate.GetLastActiveTime() - base::Time::UnixEpoch())
          .InMilliseconds());

  const int current_index = tab_delegate.GetCurrentEntryIndex();
  const int min_index = std::max(0, current_index - kMaxSyncNavigationCount);
  const int max_index = std::min(current_index + kMaxSyncNavigationCount,
                                 tab_delegate.GetEntryCount());
  bool has_child_account = tab_delegate.ProfileHasChildAccount();

  for (int i = min_index; i < max_index; ++i) {
    if (!tab_delegate.GetVirtualURLAtIndex(i).is_valid()) {
      continue;
    }
    sessions::SerializedNavigationEntry serialized_entry;
    tab_delegate.GetSerializedNavigationAtIndex(i, &serialized_entry);

    // Set current_navigation_index to the index in navigations.
    if (i == current_index) {
      specifics.set_current_navigation_index(specifics.navigation_size());
    }

    sync_pb::TabNavigation* navigation = specifics.add_navigation();
    SessionNavigationToSyncData(serialized_entry).Swap(navigation);
  }

  // If the current navigation is invalid, set the index to the end of the
  // navigation array.
  if (specifics.current_navigation_index() < 0) {
    specifics.set_current_navigation_index(specifics.navigation_size() - 1);
  }

  if (has_child_account) {
    const std::vector<std::unique_ptr<const SerializedNavigationEntry>>*
        blocked_navigations = tab_delegate.GetBlockedNavigations();

    if (blocked_navigations) {
      for (const auto& entry_unique_ptr : *blocked_navigations) {
        sync_pb::TabNavigation* navigation = specifics.add_navigation();
        SessionNavigationToSyncData(*entry_unique_ptr).Swap(navigation);
      }
    }
  }

  if (window_delegate) {
    specifics.set_browser_type(BrowserTypeFromWindowDelegate(*window_delegate));
  }

  return specifics;
}

bool LocalSessionEventHandlerImpl::AssociatePlaceholderTab(
    std::unique_ptr<SyncedTabDelegate> snapshot,
    WriteBatch* batch) {
  // In the event the data read fails or there is no persisted data, a nullptr
  // will have been returned and this should early exit.
  if (!snapshot) {
    return false;
  }

  const SessionID tab_id = snapshot->GetSessionId();
  const SessionID window_id = snapshot->GetWindowId();

  // If for some reason the tab ID or the window ID is invalid, skip it.
  if (!tab_id.is_valid() || !window_id.is_valid()) {
    return false;
  }

  AssociateTab(snapshot.get(), batch);
  return true;
}

}  // namespace sync_sessions
