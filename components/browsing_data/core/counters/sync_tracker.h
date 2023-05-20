// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_SYNC_TRACKER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_SYNC_TRACKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/sync/service/sync_service_observer.h"

namespace browsing_data {

// A helper class that subscribes to sync changes and notifies
// |counter| when the sync state changes.
class BrowsingDataCounter;

class SyncTracker : public syncer::SyncServiceObserver {
 public:
  using SyncPredicate =
      base::RepeatingCallback<bool(const syncer::SyncService*)>;

  SyncTracker(BrowsingDataCounter* counter, syncer::SyncService* sync_service);
  ~SyncTracker() override;

  void OnInitialized(SyncPredicate predicate);

  bool IsSyncActive();

 private:
  // SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  raw_ptr<BrowsingDataCounter> counter_;
  raw_ptr<syncer::SyncService> sync_service_;
  SyncPredicate predicate_;
  bool sync_enabled_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_SYNC_TRACKER_H_
