// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_

#include "base/observer_list.h"
#include "components/data_sharing/migration/public/migratable_sync_service_coordinator.h"

namespace data_sharing {

class MigratableSyncService;

class MigratableSyncServiceCoordinatorImpl
    : public MigratableSyncServiceCoordinator {
 public:
  MigratableSyncServiceCoordinatorImpl();
  ~MigratableSyncServiceCoordinatorImpl() override;

  // MigratableSyncServiceCoordinator implementation:
  void RegisterService(MigratableSyncService* service) override;
  void UnregisterService(MigratableSyncService* service) override;

 private:
  // List of all registered services that can be migrated.
  // The `check_empty=true` flag ensures all services unregister on shutdown.
  base::ObserverList<MigratableSyncService, /*check_empty=*/true> services_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_
