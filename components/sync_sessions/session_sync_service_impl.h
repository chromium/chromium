// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/model_type_store.h"
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
  ~SessionSyncServiceImpl() override;

  syncer::GlobalIdMapper* GetGlobalIdMapper() const override;

  // Return the active OpenTabsUIDelegate. If open/proxy tabs is not enabled or
  // not currently syncing, returns nullptr.
  OpenTabsUIDelegate* GetOpenTabsUIDelegate() override;

  // Allows client code to be notified when foreign sessions change.
  std::unique_ptr<base::CallbackList<void()>::Subscription>
  SubscribeToForeignSessionsChanged(const base::RepeatingClosure& cb) override
      WARN_UNUSED_RESULT;

  // For ProfileSyncService to initialize the controller for SESSIONS.
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override;

  // Intended to be used by ProxyDataTypeController: influences whether
  // GetOpenTabsUIDelegate() returns null or not.
  void ProxyTabsStateChanged(syncer::DataTypeController::State state) override;

  // Used on Android only, to override the machine tag.
  void SetSyncSessionsGUID(const std::string& guid) override;

  // Returns OpenTabsUIDelegate regardless of sync being enabled or disabled,
  // useful for tests.
  OpenTabsUIDelegate* GetUnderlyingOpenTabsUIDelegateForTest();

  SyncSessionsClient* GetSessionsClientForTest() {
    return sessions_client_.get();
  }

 private:
  void NotifyForeignSessionUpdated();

  std::unique_ptr<SyncSessionsClient> sessions_client_;

  bool proxy_tabs_running_ = false;

  std::unique_ptr<SessionSyncBridge> bridge_;

  base::CallbackList<void()> foreign_sessions_changed_callback_list_;

  DISALLOW_COPY_AND_ASSIGN(SessionSyncServiceImpl);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_IMPL_H_
