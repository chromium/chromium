// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_OBSERVER_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_OBSERVER_H_

namespace syncer {

class SyncService;

// Various UI components such as the New Tab page can be driven by observing
// the SyncService through this interface.
class SyncServiceObserver {
 public:
  // When one of the following events occurs, OnStateChanged() is called.
  // Observers should query the service to determine what happened.
  // - We initialized successfully.
  // - The sync servers are unavailable at this time.
  // - Credentials are now in flight for authentication.
  // - The data type configuration has started or ended.
  // - Sync shut down.
  // - Sync errors (passphrase, auth, unrecoverable, actionable, etc.).
  // - Encryption changes.
  virtual void OnStateChanged(SyncService* sync) {}

  // Invoked when the state of whether payments integration is enabled or not
  // changed (user setting change or via enterprise policy).
  // TODO(crbug.com/1459963): Revisit this observer function: either remove or
  // replace it with a more general-purpose version (e.g. preferred types
  // changed).
  virtual void OnSyncPaymentsIntegrationEnabledChanged(SyncService* sync) {}

  // If a client wishes to handle sync cycle completed events in a special way,
  // they can use this function.  By default, it re-routes to OnStateChanged().
  virtual void OnSyncCycleCompleted(SyncService* sync);

  // Called when the sync service has finished the datatype configuration
  // process.
  virtual void OnSyncConfigurationCompleted(SyncService* sync) {}

  // Called when the sync service is being shutdown permanently, so that
  // longer-lived observers can drop references to it.
  virtual void OnSyncShutdown(SyncService* sync) {}

 protected:
  SyncServiceObserver() = default;
  virtual ~SyncServiceObserver() = default;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_OBSERVER_H_
