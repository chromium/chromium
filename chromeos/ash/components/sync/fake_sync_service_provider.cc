// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sync/fake_sync_service_provider.h"

#include "base/check.h"

namespace ash {

FakeSyncServiceProvider::FakeSyncServiceProvider() = default;

FakeSyncServiceProvider::~FakeSyncServiceProvider() = default;

void FakeSyncServiceProvider::SetSyncServiceForAccount(
    const AccountId& account_id,
    syncer::SyncService* sync_service) {
  if (sync_service) {
    auto [it, inserted] = sync_services_.try_emplace(account_id, sync_service);
    CHECK(inserted);
  } else {
    sync_services_.erase(account_id);
  }
}

syncer::SyncService* FakeSyncServiceProvider::Find(
    const AccountId& account_id) {
  auto it = sync_services_.find(account_id);
  return it == sync_services_.end() ? nullptr : it->second;
}

}  // namespace ash
