// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/safe_browsing_sync_observer_impl.h"

namespace safe_browsing {

SafeBrowsingSyncObserverImpl::SafeBrowsingSyncObserverImpl(
    syncer::SyncService* sync_service) {
  // sync can be null in tests and in Incognito.
  if (sync_service) {
    sync_service_observer_.Observe(sync_service);
    is_sync_feature_enabled_ = sync_service->IsSyncFeatureEnabled();
  }
}

SafeBrowsingSyncObserverImpl::~SafeBrowsingSyncObserverImpl() = default;

void SafeBrowsingSyncObserverImpl::ObserveSyncStateChanged(Callback callback) {
  callback_ = std::move(callback);
}

void SafeBrowsingSyncObserverImpl::OnStateChanged(syncer::SyncService* sync) {
  bool is_sync_feature_enabled = sync->IsSyncFeatureEnabled();
  if (is_sync_feature_enabled == is_sync_feature_enabled_) {
    return;
  }
  is_sync_feature_enabled_ = is_sync_feature_enabled;
  if (!callback_.is_null()) {
    callback_.Run();
  }
}

void SafeBrowsingSyncObserverImpl::OnSyncShutdown(syncer::SyncService* sync) {
  sync_service_observer_.Reset();
}

}  // namespace safe_browsing
