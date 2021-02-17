// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_
#define COMPONENTS_SYNC_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "components/sync/protocol/session_specifics.pb.h"
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
  ~TestSyncedTabDelegate() override;

  void Navigate(const std::string& url,
                base::Time time = base::Time::Now(),
                ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED);
  void set_current_entry_index(int i);
  void set_blocked_navigations(
      const std::vector<std::unique_ptr<sessions::SerializedNavigationEntry>>&
          navs);

  void SetPageLanguageAtIndex(int i, const std::string& language);

  // SyncedTabDelegate overrides.
  bool IsInitialBlankNavigation() const override;
  int GetCurrentEntryIndex() const override;
  GURL GetVirtualURLAtIndex(int i) const override;
  GURL GetFaviconURLAtIndex(int i) const override;
  ui::PageTransition GetTransitionAtIndex(int i) const override;
  std::string GetPageLanguageAtIndex(int i) const override;
  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override;
  int GetEntryCount() const override;
  SessionID GetWindowId() const override;
  SessionID GetSessionId() const override;
  bool IsBeingDestroyed() const override;
  std::string GetExtensionAppId() const override;
  bool ProfileIsSupervised() const override;
  void set_is_supervised(bool is_supervised);
  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const override;
  bool IsPlaceholderTab() const override;
  bool ShouldSync(SyncSessionsClient* sessions_client) override;
  int64_t GetTaskIdForNavigationId(int nav_id) const override;
  int64_t GetParentTaskIdForNavigationId(int nav_id) const override;
  int64_t GetRootTaskIdForNavigationId(int nav_id) const override;

 private:
  const SessionID window_id_;
  const SessionID tab_id_;
  const base::RepeatingCallback<void(SyncedTabDelegate*)> notify_cb_;

  int current_entry_index_ = -1;
  bool is_supervised_ = false;
  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      blocked_navigations_;
  std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>
      entries_;
  std::vector<std::string> page_language_per_index_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncedTabDelegate);
};

// A placeholder delegate. These delegates have no WebContents, simulating a tab
// that has been restored without bringing its state fully into memory (for
// example on Android), or where the tab's contents have been evicted from
// memory. See SyncedTabDelegate::IsPlaceHolderTab for more info.
class PlaceholderTabDelegate : public SyncedTabDelegate {
 public:
  explicit PlaceholderTabDelegate(SessionID tab_id);
  ~PlaceholderTabDelegate() override;

  // SyncedTabDelegate overrides.
  SessionID GetSessionId() const override;
  bool IsPlaceholderTab() const override;
  // Everything else is invalid to invoke as it depends on a valid WebContents.
  SessionID GetWindowId() const override;
  bool IsBeingDestroyed() const override;
  std::string GetExtensionAppId() const override;
  bool IsInitialBlankNavigation() const override;
  int GetCurrentEntryIndex() const override;
  int GetEntryCount() const override;
  GURL GetVirtualURLAtIndex(int i) const override;
  GURL GetFaviconURLAtIndex(int i) const override;
  ui::PageTransition GetTransitionAtIndex(int i) const override;
  std::string GetPageLanguageAtIndex(int i) const override;
  void GetSerializedNavigationAtIndex(
      int i,
      sessions::SerializedNavigationEntry* serialized_entry) const override;
  bool ProfileIsSupervised() const override;
  const std::vector<std::unique_ptr<const sessions::SerializedNavigationEntry>>*
  GetBlockedNavigations() const override;
  bool ShouldSync(SyncSessionsClient* sessions_client) override;
  int64_t GetTaskIdForNavigationId(int nav_id) const override;
  int64_t GetParentTaskIdForNavigationId(int nav_id) const override;
  int64_t GetRootTaskIdForNavigationId(int nav_id) const override;

 private:
  const SessionID tab_id_;

  DISALLOW_COPY_AND_ASSIGN(PlaceholderTabDelegate);
};

class TestSyncedWindowDelegate : public SyncedWindowDelegate {
 public:
  explicit TestSyncedWindowDelegate(SessionID window_id,
                                    sync_pb::SessionWindow_BrowserType type);
  ~TestSyncedWindowDelegate() override;

  // |delegate| must not be nullptr and must outlive this object.
  void OverrideTabAt(int index, SyncedTabDelegate* delegate);

  void CloseTab(SessionID tab_id);

  void SetIsSessionRestoreInProgress(bool value);

  // SyncedWindowDelegate overrides.
  bool HasWindow() const override;
  SessionID GetSessionId() const override;
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  bool IsTypeNormal() const override;
  bool IsTypePopup() const override;
  bool IsTabPinned(const SyncedTabDelegate* tab) const override;
  SyncedTabDelegate* GetTabAt(int index) const override;
  SessionID GetTabIdAt(int index) const override;
  bool IsSessionRestoreInProgress() const override;
  bool ShouldSync() const override;

 private:
  const SessionID window_id_;
  const sync_pb::SessionWindow_BrowserType window_type_;

  std::vector<SyncedTabDelegate*> tab_delegates_;
  bool is_session_restore_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncedWindowDelegate);
};

class TestSyncedWindowDelegatesGetter : public SyncedWindowDelegatesGetter {
 public:
  TestSyncedWindowDelegatesGetter();
  ~TestSyncedWindowDelegatesGetter() override;

  void ResetWindows();
  TestSyncedWindowDelegate* AddWindow(
      sync_pb::SessionWindow_BrowserType type,
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
  const SyncedWindowDelegate* FindById(SessionID id) override;

 private:
  class DummyRouter : public LocalSessionEventRouter {
   public:
    DummyRouter();
    ~DummyRouter() override;
    void StartRoutingTo(LocalSessionEventHandler* handler) override;
    void Stop() override;
    void NotifyNav(SyncedTabDelegate* tab);
    void NotifySessionRestoreComplete();

   private:
    LocalSessionEventHandler* handler_ = nullptr;
  };

  SyncedWindowDelegateMap delegates_;
  std::vector<std::unique_ptr<TestSyncedWindowDelegate>> windows_;
  std::vector<std::unique_ptr<TestSyncedTabDelegate>> tabs_;
  DummyRouter router_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncedWindowDelegatesGetter);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_TEST_SYNCED_WINDOW_DELEGATES_GETTER_H_
