// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_CLIENT_H_
#define COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/sync/model/data_type_store.h"

class GURL;

namespace sync_sessions {

class LocalSessionEventRouter;
class SessionSyncPrefs;
class SyncedWindowDelegatesGetter;

// Interface for clients of a sync sessions datatype. Should be used as a getter
// for services and data the Sync Sessions datatype depends on.
class SyncSessionsClient {
 public:
  SyncSessionsClient();

  SyncSessionsClient(const SyncSessionsClient&) = delete;
  SyncSessionsClient& operator=(const SyncSessionsClient&) = delete;

  virtual ~SyncSessionsClient();

  // Getters for services that sessions depends on.
  virtual SessionSyncPrefs* GetSessionSyncPrefs() = 0;
  virtual syncer::RepeatingDataTypeStoreFactory GetStoreFactory() = 0;

  // Clears all on demand favicons (downloaded based on synced history data).
  virtual void ClearAllOnDemandFavicons() = 0;

  // Checks if the given url is considered interesting enough to sync. Most urls
  // are considered interesting. Examples of ones that are not are invalid urls,
  // files, and chrome internal pages.
  // TODO(zea): make this a standalone function if the url constants are
  // componentized.
  virtual bool ShouldSyncURL(const GURL& url) const = 0;

  // Returns if the provided |cache_guid| is the local device's current cache\
  // GUID or is known to have been used in the past as local device GUID.
  virtual bool IsRecentLocalCacheGuid(const std::string& cache_guid) const = 0;

  // Returns the SyncedWindowDelegatesGetter for this client.
  virtual SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter() = 0;

  // Returns a LocalSessionEventRouter instance that is customized for the
  // embedder's context.
  virtual LocalSessionEventRouter* GetLocalSessionEventRouter() = 0;

  // Returns a weak pointer to the implementation instance.
  virtual base::WeakPtr<SyncSessionsClient> AsWeakPtr() = 0;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_CLIENT_H_
