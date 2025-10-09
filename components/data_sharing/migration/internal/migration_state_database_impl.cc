// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/internal/migration_state_database_impl.h"

namespace data_sharing {

MigrationStateDatabaseImpl::MigrationStateDatabaseImpl(
    const base::FilePath& profile_dir) {
  // TODO(haileywang): Implement this.
}

MigrationStateDatabaseImpl::~MigrationStateDatabaseImpl() = default;

void MigrationStateDatabaseImpl::Init(InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(haileywang): Implement this.
  is_initialized_ = true;
  std::move(callback).Run(false);
}

std::optional<data_sharing_pb::MigrationState>
MigrationStateDatabaseImpl::GetMigrationState(
    const ContextId& context_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_initialized_);
  // TODO(haileywang): Implement this.
  return std::nullopt;
}

void MigrationStateDatabaseImpl::UpdateMigrationState(
    const ContextId& context_id,
    const data_sharing_pb::MigrationState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(haileywang): Implement this.
}

void MigrationStateDatabaseImpl::DeleteMigrationState(
    const ContextId& context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(haileywang): Implement this.
}

}  // namespace data_sharing
