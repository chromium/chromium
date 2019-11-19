// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_HANDLER_IMPL_H_
#define COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_HANDLER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
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
    virtual ~WriteBatch();
    virtual void Delete(int tab_node_id) = 0;
    virtual void Put(std::unique_ptr<sync_pb::SessionSpecifics> specifics) = 0;
    virtual void Commit() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(WriteBatch);
  };

  class Delegate {
   public:
    virtual ~Delegate();
    virtual std::unique_ptr<WriteBatch> CreateLocalSessionWriteBatch() = 0;
    virtual bool IsTabNodeUnsynced(int tab_node_id) = 0;
    // Analogous to SessionsGlobalIdMapper.
    virtual void TrackLocalNavigationId(base::Time timestamp,
                                        int unique_id) = 0;
    // Analogous to the functions in FaviconCache.
    virtual void OnPageFaviconUpdated(const GURL& page_url) = 0;
    virtual void OnFaviconVisited(const GURL& page_url,
                                  const GURL& favicon_url) = 0;
  };

  // Raw pointers must not be null and all pointees must outlive this object.
  // A side effect of this constructor could include (unless session restore is
  // ongoing) the creation of a write batch (via |delegate| and committing
  // changes).
  LocalSessionEventHandlerImpl(Delegate* delegate,
                               SyncSessionsClient* sessions_client,
                               SyncedSessionTracker* session_tracker);
  ~LocalSessionEventHandlerImpl() override;

  // LocalSessionEventHandler implementation.
  void OnSessionRestoreComplete() override;
  void OnLocalTabModified(SyncedTabDelegate* modified_tab) override;
  void OnFaviconsChanged(const std::set<GURL>& page_urls,
                         const GURL& icon_url) override;

  // Returns tab specifics from |tab_delegate|. Exposed publicly for testing.
  sync_pb::SessionTab GetTabSpecificsFromDelegateForTest(
      const SyncedTabDelegate& tab_delegate) const;

 private:
  enum ReloadTabsOption { RELOAD_TABS, DONT_RELOAD_TABS };

  void CleanupLocalTabs(WriteBatch* batch);

  void AssociateWindows(ReloadTabsOption option,
                        WriteBatch* batch);

  // Loads and reassociates the local tab referenced in |tab|.
  // |batch| must not be null. This function will append necessary
  // changes for processing later.
  void AssociateTab(SyncedTabDelegate* const tab,
                    WriteBatch* batch);

  // Set |session_tab| from |tab_delegate|.
  sync_pb::SessionTab GetTabSpecificsFromDelegate(
      const SyncedTabDelegate& tab_delegate) const;

  // Update |tab_specifics| with the corresponding task ids.
  static void WriteTasksIntoSpecifics(sync_pb::SessionTab* tab_specifics,
                                      SyncedTabDelegate* tab_delegate);

  // Injected dependencies (not owned).
  Delegate* const delegate_;
  SyncSessionsClient* const sessions_client_;
  SyncedSessionTracker* const session_tracker_;

  std::string current_session_tag_;

  DISALLOW_COPY_AND_ASSIGN(LocalSessionEventHandlerImpl);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_LOCAL_SESSION_EVENT_HANDLER_IMPL_H_
