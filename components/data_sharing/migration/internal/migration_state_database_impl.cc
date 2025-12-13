// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/internal/migration_state_database_impl.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"

namespace data_sharing {

namespace {

constexpr char kMigrationStateTableName[] = "migration_state";
constexpr int kCurrentSchemaVersion = 1;
// TODO(crbug.com/301390275): make this consistent with other places where
// amount of groups are limited once numbers are known.
constexpr size_t kMaxNumEntriesInDB = 20000;

bool InitOnDBSequence(
    base::FilePath profile_dir,
    sql::Database* db,
    sqlite_proto::ProtoTableManager* table_manager,
    sqlite_proto::KeyValueData<data_sharing_pb::MigrationState>*
        migration_state_data) {
  CHECK(db);
  CHECK(table_manager);
  CHECK(migration_state_data);

  base::FilePath db_dir = profile_dir.Append(FILE_PATH_LITERAL("DataSharing"));
  if (!base::CreateDirectory(db_dir)) {
    LOG(ERROR) << "Failed to create or open DB directory: " << db_dir.value();
    return false;
  }

  const base::FilePath db_path =
      db_dir.Append(FILE_PATH_LITERAL("MigrationStateDatabase"));
  if (!db->Open(db_path)) {
    LOG(ERROR) << "Failed to open DB " << db_path << ": "
               << db->GetErrorMessage();
    return false;
  }

  table_manager->InitializeOnDbSequence(
      db, std::vector<std::string>{kMigrationStateTableName},
      kCurrentSchemaVersion);
  migration_state_data->InitializeOnDBSequence();
  return true;
}

}  // namespace

MigrationStateDatabaseImpl::MigrationStateDatabaseImpl(
    const base::FilePath& profile_dir)
    : profile_path_(profile_dir),
      db_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      db_(std::make_unique<sql::Database>(
          sql::Database::Tag("DataSharingMigrationStateStorage"))),
      proto_table_manager_(
          base::MakeRefCounted<sqlite_proto::ProtoTableManager>(
              db_task_runner_)),
      migration_state_table_(
          std::make_unique<
              sqlite_proto::KeyValueTable<data_sharing_pb::MigrationState>>(
              kMigrationStateTableName)),
      migration_state_data_(
          std::make_unique<
              sqlite_proto::KeyValueData<data_sharing_pb::MigrationState>>(
              proto_table_manager_,
              migration_state_table_.get(),
              kMaxNumEntriesInDB,
              /*flush_delay=*/base::TimeDelta())) {}

MigrationStateDatabaseImpl::~MigrationStateDatabaseImpl() {
  // Shutdown `proto_table_manager_` and delete it together with `db_` and
  // KeyValueTable on DB sequence, then deletes KeyValueData and runs
  // `shutdown_callback_` on the main sequence.
  // This ensures that DB objects outlive any other task posted to DB sequence,
  // since their deletion is the very last posted task.
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<sql::Database> db,
             scoped_refptr<sqlite_proto::ProtoTableManager> table_manager,
             auto migration_state_table) { table_manager->WillShutdown(); },
          std::move(db_), std::move(proto_table_manager_),
          std::move(migration_state_table_)));
}

void MigrationStateDatabaseImpl::Init(InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unretained() for DB objects is safe here, because `this` destructor ensures
  // that these objects outlive any task posted to DB sequence.
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitOnDBSequence, profile_path_,
                     base::Unretained(db_.get()),
                     base::Unretained(proto_table_manager_.get()),
                     base::Unretained(migration_state_data_.get())),
      base::BindOnce(&MigrationStateDatabaseImpl::OnInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::optional<data_sharing_pb::MigrationState>
MigrationStateDatabaseImpl::GetMigrationState(
    const ContextId& context_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_initialized_);
  data_sharing_pb::MigrationState state;
  if (!migration_state_data_->TryGetData(context_id.value(), &state)) {
    return std::nullopt;
  }
  return state;
}

void MigrationStateDatabaseImpl::UpdateMigrationState(
    const ContextId& context_id,
    const data_sharing_pb::MigrationState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_initialized_);
  migration_state_data_->UpdateData(context_id.value(), state);
}

void MigrationStateDatabaseImpl::DeleteMigrationState(
    const ContextId& context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_initialized_);
  migration_state_data_->DeleteData({context_id.value()});
}

void MigrationStateDatabaseImpl::OnInitialized(InitCallback callback,
                                               bool success) {
  is_initialized_ = success;
  std::move(callback).Run(success);
}

}  // namespace data_sharing
