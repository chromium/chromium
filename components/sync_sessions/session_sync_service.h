// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_type_store.h"
#include "components/version_info/channel.h"

namespace syncer {
class GlobalIdMapper;
class ModelTypeControllerDelegate;
class SyncableService;
}  // namespace syncer

namespace sync_sessions {

class AbstractSessionsSyncManager;
class FaviconCache;
class OpenTabsUIDelegate;
class SyncSessionsClient;

// KeyedService responsible session sync (aka tab sync), including favicon sync.
// This powers things like the history UI, where "Tabs from other devices"
// exists, as well as the uploading counterpart for other devices to see our
// local tabs.
class SessionSyncService : public KeyedService {
 public:
  SessionSyncService(version_info::Channel channel,
                     std::unique_ptr<SyncSessionsClient> sessions_client);
  ~SessionSyncService() override;

  syncer::GlobalIdMapper* GetGlobalIdMapper() const;

  // Intended for ProfileSyncService: returns the OpenTabsUIDelegate instance,
  // which is guaranteed to be non-null (independently of whether sync is
  // running or not)
  OpenTabsUIDelegate* GetRawOpenTabsUIDelegate();

  // Schedules garbage collection of foreign sessions.
  void ScheduleGarbageCollection();

  // For ProfileSyncService to initialize the controller for SESSIONS. Exactly
  // one of the two below will return non-null (depending on a feature toggle).
  syncer::SyncableService* GetSyncableService();
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate();

  // For ProfileSyncService to initialize the controller for FAVICON_IMAGES and
  // FAVICON_TRACKING.
  FaviconCache* GetFaviconCache();

  // Used on Android only, to override the machine tag.
  void SetSyncSessionsGUID(const std::string& guid);

 private:
  std::unique_ptr<SyncSessionsClient> sessions_client_;

  // Locally owned SyncableService or ModelTypeSyncBridge implementations.
  std::unique_ptr<sync_sessions::AbstractSessionsSyncManager>
      sessions_sync_manager_;

  DISALLOW_COPY_AND_ASSIGN(SessionSyncService);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_SERVICE_H_
