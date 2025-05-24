// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/version_info/channel.h"

namespace sync_sessions {

class SessionSyncBridge;
class SyncSessionsClient;

// Single non-test implementation of SessionSyncService.
class SessionSyncServiceImpl : public SessionSyncService {
 public:
  SessionSyncServiceImpl(version_info::Channel channel,
                         std::unique_ptr<SyncSessionsClient> sessions_client);

  SessionSyncServiceImpl(const SessionSyncServiceImpl&) = delete;
  SessionSyncServiceImpl& operator=(const SessionSyncServiceImpl&) = delete;

  ~SessionSyncServiceImpl() override;

  // SessionSyncService overrides.
  syncer::GlobalIdMapper* GetGlobalIdMapper() const override;
  OpenTabsUIDelegate* GetOpenTabsUIDelegate() override;
  [[nodiscard]] base::CallbackListSubscription
  SubscribeToForeignSessionsChanged(const base::RepeatingClosure& cb) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

 private:
  void NotifyForeignSessionUpdated();

  std::unique_ptr<SyncSessionsClient> sessions_client_;

  std::unique_ptr<SessionSyncBridge> bridge_;

  base::RepeatingClosureList foreign_sessions_changed_closure_list_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_IMPL_H_
