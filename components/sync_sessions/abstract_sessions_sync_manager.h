// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_ABSTRACT_SESSIONS_SYNC_MANAGER_H_
#define COMPONENTS_SYNC_SESSIONS_ABSTRACT_SESSIONS_SYNC_MANAGER_H_

#include "base/macros.h"

namespace syncer {
class ModelTypeSyncBridge;
class SyncableService;
}  // namespace syncer

namespace sync_sessions {

class FaviconCache;
class OpenTabsUIDelegate;
class SessionsGlobalIdMapper;

// TODO(crbug.com/895455): Remove this interface once the migration to USS
// is completed and the old code removed.
class AbstractSessionsSyncManager {
 public:
  AbstractSessionsSyncManager();
  virtual ~AbstractSessionsSyncManager();

  virtual void ScheduleGarbageCollection() = 0;
  virtual FaviconCache* GetFaviconCache() = 0;
  virtual SessionsGlobalIdMapper* GetGlobalIdMapper() = 0;
  virtual OpenTabsUIDelegate* GetOpenTabsUIDelegate() = 0;
  // Exactly one of the two below returns nullptr.
  virtual syncer::SyncableService* GetSyncableService() = 0;
  virtual syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractSessionsSyncManager);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_ABSTRACT_SESSIONS_SYNC_MANAGER_H_
