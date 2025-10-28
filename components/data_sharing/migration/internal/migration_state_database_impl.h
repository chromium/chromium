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
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "sql/database.h"

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
  void OnInitialized(InitCallback callback, bool success);

  // The file path of the profile directory.
  const base::FilePath profile_path_;

  // The following fields hold objects to work with SQLite database. `db_`,
  // `proto_table_manager_` are deleted on db sequence; `migration_state_data_`
  // and `migration_state_table_` are deleted on the main thread, however only
  // after deletion of the rest.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  std::unique_ptr<sql::Database> db_;
  scoped_refptr<sqlite_proto::ProtoTableManager> proto_table_manager_;
  // Entities are keyed by ContextID.
  std::unique_ptr<sqlite_proto::KeyValueTable<data_sharing_pb::MigrationState>>
      migration_state_table_;
  std::unique_ptr<sqlite_proto::KeyValueData<data_sharing_pb::MigrationState>>
      migration_state_data_;

  // Whether database is initialized.
  bool is_initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MigrationStateDatabaseImpl> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_INTERNAL_MIGRATION_STATE_DATABASE_IMPL_H_
