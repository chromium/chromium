// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_
#define COMPONENTS_SYNC_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_sessions/local_session_event_router.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace sync_sessions {

// A SyncedTabDelegate fake for testing. It simulates a normal
// SyncedTabDelegate with a proper WebContents. For a SyncedTabDelegate without
// a WebContents, see PlaceholderTabDelegate below.
class TestSyncedTabDelegate : public SyncedTabDelegate {
 public:
  TestSyncedTabDelegate(
      SessionID window_id,
      SessionID tab_id,
      const base::RepeatingCallback<void(SyncedTabDelegate*)>& notify_cb);

  TestSyncedTabDelegate(const TestSyncedTabDelegate&) = delete;
  TestSyncedTabDelegate& operator=(const TestSyncedTabDelegate&) = delete;

  ~TestSyncedTabDelegate() override;

  void Navigate(const std::string& url,
                base::Time time = base::Time::Now(),
                ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED);
  void set_current_entry_index(int i);
  void set_blocked_navigations(
      const std::vector<std::unique_ptr<sessions::SerializedNavigationEntry>>&
          navs);

  // SyncedTabDelegate overrides.
  bool IsInitialBlankNavigation() const override;
  int GetCurrentEntryIndex() const override;
  GURL GetVirtualURLAtIndex(int i) const override;
  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override;
  int GetEntryCount() const override;
  SessionID GetWindowId() const override;
  SessionID GetSessionId() const override;
  bool IsBeingDestroyed() const override;
  base::Time GetLastActiveTime() override;
  std::string GetExtensionAppId() const override;
  bool ProfileHasChildAccount() const override;
  void set_has_child_account(bool has_child_account);
  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const override;
  bool IsPlaceholderTab() const override;
  bool ShouldSync(SyncSessionsClient* sessions_client) override;
  int64_t GetTaskIdForNavigationId(int nav_id) const override;
  int64_t GetParentTaskIdForNavigationId(int nav_id) const override;
  int64_t GetRootTaskIdForNavigationId(int nav_id) const override;
  std::unique_ptr<SyncedTabDelegate> ReadPlaceholderTabSnapshotIfItShouldSync(
      SyncSessionsClient* sessions_client) override;

 private:
  const SessionID window_id_;
  const SessionID tab_id_;
  const base::RepeatingCallback<void(SyncedTabDelegate*)> notify_cb_;

  int current_entry_index_ = -1;
  bool has_child_account_ = false;
  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      blocked_navigations_;
  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      entries_;
};

// A placeholder delegate. These delegates have no WebContents, simulating a tab
// that has been restored without bringing its state fully into memory (for
// example on Android), or where the tab's contents have been evicted from
// memory. See SyncedTabDelegate::IsPlaceHolderTab for more info.
class PlaceholderTabDelegate : public SyncedTabDelegate {
 public:
  explicit PlaceholderTabDelegate(SessionID tab_id);

  PlaceholderTabDelegate(const PlaceholderTabDelegate&) = delete;
  PlaceholderTabDelegate& operator=(const PlaceholderTabDelegate&) = delete;

  ~PlaceholderTabDelegate() override;

  void SetPlaceholderTabSyncedTabDelegate(
      std::unique_ptr<SyncedTabDelegate> delegate);

  // SyncedTabDelegate overrides.
  SessionID GetSessionId() const override;
  bool IsPlaceholderTab() const override;
  std::unique_ptr<SyncedTabDelegate> ReadPlaceholderTabSnapshotIfItShouldSync(
      SyncSessionsClient* sessions_client) override;
  // Everything else is invalid to invoke as it depends on a valid WebContents.
  SessionID GetWindowId() const override;
  bool IsBeingDestroyed() const override;
  base::Time GetLastActiveTime() override;
  std::string GetExtensionAppId() const override;
  bool IsInitialBlankNavigation() const override;
  int GetCurrentEntryIndex() const override;
  int GetEntryCount() const override;
  GURL GetVirtualURLAtIndex(int i) const override;
  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override;
  bool ProfileHasChildAccount() const override;
  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const override;
  bool ShouldSync(SyncSessionsClient* sessions_client) override;
  int64_t GetTaskIdForNavigationId(int nav_id) const override;
  int64_t GetParentTaskIdForNavigationId(int nav_id) const override;
  int64_t GetRootTaskIdForNavigationId(int nav_id) const override;

 private:
  const SessionID tab_id_;
  std::unique_ptr<SyncedTabDelegate> placeholder_tab_synced_tab_delegate_;
};

class TestSyncedWindowDelegate : public SyncedWindowDelegate {
 public:
  explicit TestSyncedWindowDelegate(SessionID window_id,
                                    sync_pb::SyncEnums_BrowserType type);

  TestSyncedWindowDelegate(const TestSyncedWindowDelegate&) = delete;
  TestSyncedWindowDelegate& operator=(const TestSyncedWindowDelegate&) = delete;

  ~TestSyncedWindowDelegate() override;

  // |delegate| must not be nullptr and must outlive this object.
  void OverrideTabAt(int index, SyncedTabDelegate* delegate);

  void CloseTab(SessionID tab_id);

  void SetIsSessionRestoreInProgress(bool value);

  // SyncedWindowDelegate overrides.
  bool HasWindow() const override;
  SessionID GetSessionId() const override;
  int GetTabCount() const override;
  bool IsTypeNormal() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const SyncedTabDelegate* tab) const override;
  SyncedTabDelegate* GetTabAt(int index) const override;
  SessionID GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

 private:
  const SessionID window_id_;
  const sync_pb::SyncEnums_BrowserType window_type_;

  std::vector<raw_ptr<SyncedTabDelegate, VectorExperimental>> tab_delegates_;
  bool is_session_restore_in_progress_;
};

class TestSyncedWindowDelegatesGetter : public SyncedWindowDelegatesGetter {
 public:
  TestSyncedWindowDelegatesGetter();

  TestSyncedWindowDelegatesGetter(const TestSyncedWindowDelegatesGetter&) =
      delete;
  TestSyncedWindowDelegatesGetter& operator=(
      const TestSyncedWindowDelegatesGetter&) = delete;

  ~TestSyncedWindowDelegatesGetter() override;

  void ResetWindows();
  TestSyncedWindowDelegate* AddWindow(
      sync_pb::SyncEnums_BrowserType type,
      SessionID window_id = SessionID::NewUnique());
  // Creates a new tab within the window specified by |window_id|. The newly
  // created tab's ID can be specified optionally. Returns the newly created
  // TestSyncedTabDelegate (not owned).
  TestSyncedTabDelegate* AddTab(SessionID window_id,
                                SessionID tab_id = SessionID::NewUnique());
  void CloseTab(SessionID tab_id);
  void SessionRestoreComplete();
  LocalSessionEventRouter* router();

  // SyncedWindowDelegatesGetter overrides.
  SyncedWindowDelegateMap GetSyncedWindowDelegates() override;
  const SyncedWindowDelegate* FindById(SessionID session_id) override;

 private:
  class TestRouter : public LocalSessionEventRouter {
   public:
    TestRouter();
    ~TestRouter() override;
    void StartRoutingTo(LocalSessionEventHandler* handler) override;
    void Stop() override;
    void NotifyNav(SyncedTabDelegate* tab);
    void NotifySessionRestoreComplete();

   private:
    raw_ptr<LocalSessionEventHandler, DanglingUntriaged> handler_ = nullptr;
  };

  SyncedWindowDelegateMap delegates_;
  std::vector<std::unique_ptr<TestSyncedWindowDelegate>> windows_;
  std::vector<std::unique_ptr<TestSyncedTabDelegate>> tabs_;
  TestRouter router_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_
