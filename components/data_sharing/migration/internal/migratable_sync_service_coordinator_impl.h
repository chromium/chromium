// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_

#include "components/data_sharing/migration/public/migratable_sync_service_coordinator.h"

namespace data_sharing {

class MigratableSyncServiceCoordinatorImpl
    : public MigratableSyncServiceCoordinator {
 public:
  MigratableSyncServiceCoordinatorImpl();
  ~MigratableSyncServiceCoordinatorImpl() override;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_
