// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SAFE_BROWSING_SYNC_OBSERVER_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SAFE_BROWSING_SYNC_OBSERVER_IMPL_H_

#include "base/scoped_observation.h"
#include "components/safe_browsing/core/browser/safe_browsing_sync_observer.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {
class SyncService;
}

namespace safe_browsing {

// This class observes sync events and notifies the observer.
class SafeBrowsingSyncObserverImpl : public SafeBrowsingSyncObserver,
                                     public syncer::SyncServiceObserver {
 public:
  explicit SafeBrowsingSyncObserverImpl(syncer::SyncService* sync_service);

  ~SafeBrowsingSyncObserverImpl() override;

  // SafeBrowsingSyncObserver:
  void ObserveHistorySyncStateChanged(Callback callback) override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  Callback callback_;
  bool is_history_sync_enabled_ = false;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SAFE_BROWSING_SYNC_OBSERVER_IMPL_H_
