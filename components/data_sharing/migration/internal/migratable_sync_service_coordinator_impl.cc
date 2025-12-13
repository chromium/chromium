// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/internal/migratable_sync_service_coordinator_impl.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "components/data_sharing/migration/internal/migration_state_database_impl.h"
#include "components/data_sharing/migration/internal/protocol/migration_state.pb.h"
#include "components/data_sharing/migration/public/migratable_sync_service.h"

namespace data_sharing {

MigratableSyncServiceCoordinatorImpl::MigratableSyncServiceCoordinatorImpl(
    const base::FilePath& profile_dir)
    : db_(std::make_unique<MigrationStateDatabaseImpl>(profile_dir)) {
  db_->Init(
      base::BindOnce(&MigratableSyncServiceCoordinatorImpl::OnDbInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

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

void MigratableSyncServiceCoordinatorImpl::OnDbInitialized(bool success) {
  NOTIMPLEMENTED();
}

void MigratableSyncServiceCoordinatorImpl::RunPendingTasks() {
  NOTIMPLEMENTED();
}

void MigratableSyncServiceCoordinatorImpl::StartSharing(
    const ContextId& context_id) {
  NOTIMPLEMENTED();
}

void MigratableSyncServiceCoordinatorImpl::FinalizeMigration(
    const ContextId& context_id) {
  NOTIMPLEMENTED();
}

void MigratableSyncServiceCoordinatorImpl::PrepareUnsharing(
    const ContextId& context_id) {
  NOTIMPLEMENTED();
}

void MigratableSyncServiceCoordinatorImpl::OnUnsharingStarted(
    const ContextId& context_id) {
  NOTIMPLEMENTED();
}

bool MigratableSyncServiceCoordinatorImpl::IsContextMidMigration(
    const ContextId& context_id) const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace data_sharing
