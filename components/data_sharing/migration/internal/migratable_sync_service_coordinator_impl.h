// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/data_sharing/migration/internal/migration_state_database.h"
#include "components/data_sharing/migration/public/migratable_sync_service_coordinator.h"

namespace data_sharing {

class MigratableSyncService;

class MigratableSyncServiceCoordinatorImpl
    : public MigratableSyncServiceCoordinator {
 public:
  explicit MigratableSyncServiceCoordinatorImpl(
      const base::FilePath& profile_dir);
  ~MigratableSyncServiceCoordinatorImpl() override;

  // MigratableSyncServiceCoordinator implementation:
  void RegisterService(MigratableSyncService* service) override;
  void UnregisterService(MigratableSyncService* service) override;
  void StartSharing(const ContextId& context_id) override;
  void FinalizeMigration(const ContextId& context_id) override;
  void PrepareUnsharing(const ContextId& context_id) override;
  void OnUnsharingStarted(const ContextId& context_id) override;
  bool IsContextMidMigration(const ContextId& context_id) const override;

 private:
  void OnDbInitialized(bool success);
  void RunPendingTasks();

  // The database for persisting migration state for crash recovery.
  std::unique_ptr<MigrationStateDatabase> db_;

  // List of all registered services that can be migrated.
  base::ObserverList<MigratableSyncService, /*check_empty=*/true> services_;

  // Queues tasks that arrive before the DB is initialized.
  std::vector<base::OnceClosure> pending_tasks_;

  base::WeakPtrFactory<MigratableSyncServiceCoordinatorImpl> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATABLE_SYNC_SERVICE_COORDINATOR_IMPL_H_
