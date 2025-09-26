// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_SERVICE_COORDINATOR_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_SERVICE_COORDINATOR_H_

#include "components/keyed_service/core/keyed_service.h"

namespace data_sharing {

class MigratableSyncService;

// A KeyedService that orchestrates the entire migration process of sync
// entities between private and shared states. It ensures data consistency, and
// tracks the state of any in-flight operations.
class MigratableSyncServiceCoordinator : public KeyedService {
 public:
  MigratableSyncServiceCoordinator() = default;
  ~MigratableSyncServiceCoordinator() override = default;

  // Disallow copy/assign.
  MigratableSyncServiceCoordinator(const MigratableSyncServiceCoordinator&) =
      delete;
  MigratableSyncServiceCoordinator& operator=(
      const MigratableSyncServiceCoordinator&) = delete;

  // Called by feature services on startup to register themselves for migration.
  virtual void RegisterService(MigratableSyncService* service) = 0;
  virtual void UnregisterService(MigratableSyncService* service) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_SERVICE_COORDINATOR_H_
