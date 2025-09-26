// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/internal/migratable_sync_service_coordinator_impl.h"

#include "base/observer_list.h"
#include "components/data_sharing/migration/public/migratable_sync_service.h"

namespace data_sharing {

MigratableSyncServiceCoordinatorImpl::MigratableSyncServiceCoordinatorImpl() =
    default;
MigratableSyncServiceCoordinatorImpl::~MigratableSyncServiceCoordinatorImpl() =
    default;

void MigratableSyncServiceCoordinatorImpl::RegisterService(
    MigratableSyncService* service) {
  services_.AddObserver(service);
}

void MigratableSyncServiceCoordinatorImpl::UnregisterService(
    MigratableSyncService* service) {
  services_.RemoveObserver(service);
}

}  // namespace data_sharing
