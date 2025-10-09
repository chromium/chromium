// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATION_STATE_DATABASE_IMPL_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATION_STATE_DATABASE_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/data_sharing/migration/internal/migration_state_database.h"
#include "components/data_sharing/migration/internal/protocol/migration_state.pb.h"
#include "components/data_sharing/migration/public/context_id.h"

namespace data_sharing {

// A SQLite database for storing data_sharing migration state. This database
// stores a proto for each sync entity, keyed by the context id. The data is
// cached in memory for synchronous reads.
class MigrationStateDatabaseImpl : public MigrationStateDatabase {
 public:
  explicit MigrationStateDatabaseImpl(const base::FilePath& profile_dir);
  ~MigrationStateDatabaseImpl() override;
  MigrationStateDatabaseImpl(const MigrationStateDatabaseImpl&) = delete;
  MigrationStateDatabaseImpl& operator=(const MigrationStateDatabaseImpl&) =
      delete;

  // MigrationStateDatabase implementation.
  void Init(InitCallback callback) override;
  std::optional<data_sharing_pb::MigrationState> GetMigrationState(
      const ContextId& context_id) const override;
  void UpdateMigrationState(
      const ContextId& context_id,
      const data_sharing_pb::MigrationState& state) override;
  void DeleteMigrationState(const ContextId& context_id) override;

 private:
  // In-memory cache of the migration state.
  std::map<ContextId, data_sharing_pb::MigrationState> cache_;

  // Whether database is initialized.
  bool is_initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MigrationStateDatabaseImpl> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATION_STATE_DATABASE_IMPL_H_
