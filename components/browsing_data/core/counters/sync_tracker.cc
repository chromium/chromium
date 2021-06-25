// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/sync_tracker.h"

#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/sync/driver/sync_service.h"

namespace browsing_data {

SyncTracker::SyncTracker(BrowsingDataCounter* counter,
                         syncer::SyncService* sync_service)
    : counter_(counter), sync_service_(sync_service), sync_enabled_() {
  DCHECK(counter_);
}

SyncTracker::~SyncTracker() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

void SyncTracker::OnInitialized(SyncPredicate predicate) {
  DCHECK(!predicate.is_null());
  predicate_ = predicate;
  if (sync_service_)
    sync_service_->AddObserver(this);
  sync_enabled_ = predicate_.Run(sync_service_);
}

bool SyncTracker::IsSyncActive() {
  return sync_enabled_;
}

void SyncTracker::OnStateChanged(syncer::SyncService* sync) {
  bool sync_enabled_new = predicate_.Run(sync_service_);

  if (sync_enabled_ != sync_enabled_new) {
    sync_enabled_ = sync_enabled_new;
    counter_->Restart();
  }
}

void SyncTracker::OnSyncShutdown(syncer::SyncService* sync) {
  DCHECK_EQ(sync_service_, sync);
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
  sync_enabled_ = false;
}

}  // namespace browsing_data
