// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/safe_browsing_sync_observer_impl.h"

#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace safe_browsing {

SafeBrowsingSyncObserverImpl::SafeBrowsingSyncObserverImpl(
    syncer::SyncService* sync_service) {
  // `sync_service` can be null in tests and in Incognito.
  if (sync_service) {
    sync_service_observer_.Observe(sync_service);
    is_history_sync_enabled_ =
        sync_service->GetUserSettings()->GetSelectedTypes().Has(
            syncer::UserSelectableType::kHistory);
  }
}

SafeBrowsingSyncObserverImpl::~SafeBrowsingSyncObserverImpl() = default;

void SafeBrowsingSyncObserverImpl::ObserveHistorySyncStateChanged(
    Callback callback) {
  callback_ = std::move(callback);
}

void SafeBrowsingSyncObserverImpl::OnStateChanged(syncer::SyncService* sync) {
  bool is_history_sync_enabled =
      sync->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory);
  if (is_history_sync_enabled == is_history_sync_enabled_) {
    return;
  }
  is_history_sync_enabled_ = is_history_sync_enabled;
  if (!callback_.is_null()) {
    callback_.Run();
  }
}

void SafeBrowsingSyncObserverImpl::OnSyncShutdown(syncer::SyncService* sync) {
  sync_service_observer_.Reset();
}

}  // namespace safe_browsing
