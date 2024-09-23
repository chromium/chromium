// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_HANDLER_IMPL_H_
#define COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_HANDLER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/local_session_event_router.h"
#include "components/sync_sessions/synced_session.h"

namespace sync_pb {
class SessionSpecifics;
class SessionTab;
}  // namespace sync_pb

namespace sync_sessions {

class SyncedSessionTracker;
class SyncedTabDelegate;
class SyncSessionsClient;

// Class responsible for propagating local session changes to the sessions
// model including SyncedSessionTracker (in-memory representation) as well as
// the persistency and sync layers (via delegate).
class LocalSessionEventHandlerImpl : public LocalSessionEventHandler {
 public:
  class WriteBatch {
   public:
    WriteBatch();

    WriteBatch(const WriteBatch&) = delete;
    WriteBatch& operator=(const WriteBatch&) = delete;

    virtual ~WriteBatch();
    virtual void Delete(int tab_node_id) = 0;
    virtual void Put(std::unique_ptr<sync_pb::SessionSpecifics> specifics) = 0;
    virtual void Commit() = 0;
  };

  class Delegate {
   public:
    virtual ~Delegate();
    virtual std::unique_ptr<WriteBatch> CreateLocalSessionWriteBatch() = 0;
    virtual bool IsTabNodeUnsynced(int tab_node_id) = 0;
    // Analogous to SessionsGlobalIdMapper.
    virtual void TrackLocalNavigationId(base::Time timestamp,
                                        int unique_id) = 0;
  };

  // Raw pointers must not be null and all pointees must outlive this object.
  // A side effect of this constructor could include (unless session restore is
  // ongoing) the creation of a write batch (via |delegate| and committing
  // changes).
  LocalSessionEventHandlerImpl(Delegate* delegate,
                               SyncSessionsClient* sessions_client,
                               SyncedSessionTracker* session_tracker,
                               bool is_new_session);

  LocalSessionEventHandlerImpl(const LocalSessionEventHandlerImpl&) = delete;
  LocalSessionEventHandlerImpl& operator=(const LocalSessionEventHandlerImpl&) =
      delete;

  ~LocalSessionEventHandlerImpl() override;

  // LocalSessionEventHandler implementation.
  void OnSessionRestoreComplete() override;
  void OnLocalTabModified(SyncedTabDelegate* modified_tab) override;

  // Returns tab specifics from |tab_delegate|. Exposed publicly for testing.
  sync_pb::SessionTab GetTabSpecificsFromDelegateForTest(
      SyncedTabDelegate& tab_delegate) const;

 private:
  enum ReloadTabsOption { RELOAD_TABS, DONT_RELOAD_TABS };

  void CleanupLocalTabs(WriteBatch* batch);

  void AssociateWindows(ReloadTabsOption option,
                        WriteBatch* batch,
                        bool is_session_restore);

  // Loads and reassociates the local tab referenced in |tab|.
  // |batch| must not be null. This function will append necessary
  // changes for processing later.
  void AssociateTab(SyncedTabDelegate* const tab, WriteBatch* batch);

  // Set |session_tab| from |tab_delegate|.
  sync_pb::SessionTab GetTabSpecificsFromDelegate(
      SyncedTabDelegate& tab_delegate) const;

  bool AssociatePlaceholderTab(std::unique_ptr<SyncedTabDelegate> snapshot,
                               WriteBatch* batch);

  // Injected dependencies (not owned).
  const raw_ptr<Delegate> delegate_;
  const raw_ptr<SyncSessionsClient> sessions_client_;
  const raw_ptr<SyncedSessionTracker> session_tracker_;

  std::string current_session_tag_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_HANDLER_IMPL_H_
