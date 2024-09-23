// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_OBSERVER_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_OBSERVER_H_

namespace syncer {

class SyncService;

// An observer that gets notified whenever any state in the SyncService changes.
// IMPORTANT: Observers must be removed before SyncService::Shutdown() gets
// called (during the KeyedServices shutdown sequence). If your observer is tied
// to a KeyedService itself, declare an appropriate DependsOn() relation and
// remove the observer in your service's Shutdown(). Otherwise, implement
// SyncServiceObserver::OnSyncShutdown() and remove the observer there.
class SyncServiceObserver {
 public:
  // When one of the following events occurs, OnStateChanged() is called.
  // Observers should query the service to determine what happened.
  // - The sync service initialized successfully.
  // - The sync servers are unavailable at this time.
  // - Credentials are now in flight for authentication.
  // - The data type configuration has started or ended.
  // - Sync shut down.
  // - Sync errors (passphrase, auth, unrecoverable, actionable, etc.).
  // - Encryption changes.
  virtual void OnStateChanged(SyncService* sync) {}

  // If a client wishes to handle sync cycle completed events in a special way,
  // they can use this function.  By default, it re-routes to OnStateChanged().
  virtual void OnSyncCycleCompleted(SyncService* sync);

  // Called when the sync service is being shutdown permanently, so that
  // longer-lived observers can drop references to it.
  virtual void OnSyncShutdown(SyncService* sync) {}

 protected:
  SyncServiceObserver() = default;
  virtual ~SyncServiceObserver() = default;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_OBSERVER_H_
