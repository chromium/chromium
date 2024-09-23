// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_database_selector.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/leveldb_proto/internal/migration_delegate.h"
#include "components/leveldb_proto/internal/shared_proto_database.h"
#include "components/leveldb_proto/internal/shared_proto_database_provider.h"
#include "components/leveldb_proto/internal/unique_proto_database.h"

namespace leveldb_proto {

namespace {

void RunInitCallbackOnTaskRunner(
    Callbacks::InitStatusCallback callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    Enums::InitStatus status) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), status));
}

}  // namespace

// static
void ProtoDatabaseSelector::RecordInitState(
    ProtoDatabaseSelector::ProtoDatabaseInitState state) {
  UMA_HISTOGRAM_ENUMERATION("ProtoDB.SharedDbInitStatus", state);
}

ProtoDatabaseSelector::ProtoDatabaseSelector(
    ProtoDbType db_type,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<SharedProtoDatabaseProvider> db_provider)
    : db_type_(db_type),
      task_runner_(task_runner),
      db_provider_(std::move(db_provider)),
      migration_delegate_(std::make_unique<MigrationDelegate>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoDatabaseSelector::~ProtoDatabaseSelector() {
  if (db_)
    task_runner_->DeleteSoon(FROM_HERE, std::move(db_));
}

void ProtoDatabaseSelector::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    db_ = std::make_unique<UniqueProtoDatabase>(task_runner_);

  unique_database_dir_ = database_dir;

  db_->InitWithDatabase(
      database, database_dir, options, false,
      base::BindOnce(&RunInitCallbackOnTaskRunner, std::move(callback),
                     callback_task_runner));
  OnInitDone(ProtoDatabaseInitState::kLegacyInitCalled);
}

void ProtoDatabaseSelector::InitUniqueOrShared(
    const std::string& client_name,
    base::FilePath db_dir,
    const leveldb_env::Options& unique_db_options,
    bool use_shared_db,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::InitStatusCallback callback) {
  RecordInitState(ProtoDatabaseInitState::kSharedDbInitAttempted);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_status_ = InitStatus::IN_PROGRESS;
  unique_database_dir_ = db_dir;
  client_name_ = client_name;

  if (unique_database_dir_.empty()) {
    DCHECK(!use_shared_db) << "Opening in memory shared db is not supported";
    // In case we set up field trials by mistake, ignore the use shared flag and
    // return unique db.
    use_shared_db = false;
  }

  auto unique_options = unique_db_options;
  // There are two Init methods, one that receives Options for its unique DB and
  // another that uses CreateSimpleOptions() to open the unique DB. In case a
  // shared DB needs to be used then we don't need to create a new unique DB if
  // it doesn't exist. In case a unique DB needs to be used then we don't change
  // the create_if_missing parameter, because it may have been set by a client.
  if (use_shared_db) {
    unique_options.create_if_missing = false;
  }

  auto unique_db = std::make_unique<UniqueProtoDatabase>(db_dir, unique_options,
                                                         task_runner_);
  auto* unique_db_ptr = unique_db.get();
  unique_db_ptr->Init(
      client_name, base::BindOnce(&ProtoDatabaseSelector::OnInitUniqueDB, this,
                                  std::move(unique_db), use_shared_db,
                                  base::BindOnce(&RunInitCallbackOnTaskRunner,
                                                 std::move(callback),
                                                 callback_task_runner)));
}

void ProtoDatabaseSelector::OnInitUniqueDB(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    Enums::InitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the unique DB is corrupt, just return it early with the corruption
  // status to avoid silently migrating a corrupt database and giving no errors
  // back.
  if (status == Enums::InitStatus::kCorrupt) {
    db_ = std::move(unique_db);
    std::move(callback).Run(Enums::InitStatus::kCorrupt);
    OnInitDone(ProtoDatabaseInitState::kFailureUniqueDbCorrupted);
    return;
  }

  // Clear out the unique_db before sending an unusable DB into InitSharedDB,
  // a nullptr indicates opening a unique DB failed.
  if (status != Enums::InitStatus::kOK)
    unique_db.reset();

  // If no SharedProtoDatabaseProvider is set then we use the unique DB (if it
  // opened correctly). If in memory db is requested then do not try to migrate
  // data from shared db, which was the behavior when only unique db existed.
  if (!db_provider_ || unique_database_dir_.empty()) {
    db_ = std::move(unique_db);
    std::move(callback).Run(status);
    OnInitDone(
        status == Enums::kOK
            ? ProtoDatabaseInitState::kSuccessNoSharedDBProviderUniqueSucceeded
            : ProtoDatabaseInitState::kFailureNoSharedDBProviderUniqueFailed);
    return;
  }

  // Get the current task runner to ensure the callback is run on the same
  // callback as the rest, and the WeakPtr checks out on the right sequence.
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  db_provider_->GetDBInstance(
      base::BindOnce(&ProtoDatabaseSelector::OnInitSharedDB, this,
                     std::move(unique_db), status, use_shared_db,
                     std::move(callback)),
      task_runner_);
}

void ProtoDatabaseSelector::OnInitSharedDB(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    Enums::InitStatus unique_db_status,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    scoped_refptr<SharedProtoDatabase> shared_db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (shared_db) {
    // If we have a reference to the shared database, try to get a client.
    shared_db->GetClientAsync(
        db_type_, use_shared_db,
        base::BindOnce(&ProtoDatabaseSelector::OnGetSharedDBClient, this,
                       std::move(unique_db), unique_db_status, use_shared_db,
                       std::move(callback)));
    return;
  }

  // Otherwise, we just call the OnGetSharedDBClient function with a nullptr
  // client.
  OnGetSharedDBClient(std::move(unique_db), unique_db_status, use_shared_db,
                      std::move(callback), nullptr, Enums::InitStatus::kError);
}

void ProtoDatabaseSelector::OnGetSharedDBClient(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    Enums::InitStatus unique_db_status,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    Enums::InitStatus shared_db_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!unique_db && !client) {
    std::move(callback).Run(Enums::InitStatus::kError);
    OnInitDone(ProtoDatabaseInitState::kBothUniqueAndSharedFailedOpen);
    return;
  }

  if (!client) {
    if (use_shared_db) {
      // If there's no shared client and one is requested we return an error,
      // because it should be created if missing.
      std::move(callback).Run(Enums::InitStatus::kError);
      OnInitDone(ProtoDatabaseInitState::kSharedDbClientMissingInitFailed);
      return;
    } else {
      // ProtoLevelDBWrapper::InitWithDatabase() returns kInvalidOperation when
      // a database doesn't exist and create_if_missing is false.
      if (shared_db_status == Enums::InitStatus::kInvalidOperation) {
        // If the shared DB doesn't exist and a unique DB is requested then we
        // return the unique DB.
        db_ = std::move(unique_db);
        std::move(callback).Run(Enums::InitStatus::kOK);
        OnInitDone(
            ProtoDatabaseInitState::kSharedDbClientMissingUniqueReturned);
        return;
      } else {
        // If the shared DB failed to open and a unique DB is requested then we
        // throw an error, as the shared DB may contain unmigrated data.
        std::move(callback).Run(Enums::InitStatus::kError);
        OnInitDone(ProtoDatabaseInitState::kSharedDbOpenFailed);
        return;
      }
    }
  }

  if (!unique_db) {
    switch (client->migration_status()) {
      case SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED:
        // ProtoLevelDBWrapper::InitWithDatabase() returns kInvalidOperation
        // when a database doesn't exist and create_if_missing is false.
        if (unique_db_status == Enums::kInvalidOperation) {
          // If the unique DB doesn't exist and the migration status is not
          // attempted then we set the status to migrated to shared and return
          // the shared DB. We don't check use_shared_db because we should only
          // get here when use_shared_db is true, otherwise the unique_db is
          // opened using create_if_missing
          client->UpdateClientInitMetadata(
              SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL);
          db_ = std::move(client);
          std::move(callback).Run(Enums::InitStatus::kOK);
          OnInitDone(ProtoDatabaseInitState::kUniqueDbMissingSharedReturned);
          return;
        }
        // If the unique DB failed to open and the migration status is not
        // attempted then we return an error, as we don't know if the unique
        // DB contains any data.
        std::move(callback).Run(Enums::InitStatus::kError);
        OnInitDone(ProtoDatabaseInitState::kUniqueDbOpenFailed);
        return;
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL:
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED:
        // If the unique DB failed to open, but the data is located in shared
        // then we use the shared DB. We do the same when the unique DB is
        // marked for deletion because there's no way to delete it.
        // use_shared_db is ignored because we know the data is in shared DB,
        // and there's no way to migrate.
        db_ = std::move(client);
        std::move(callback).Run(Enums::InitStatus::kOK);
        OnInitDone(ProtoDatabaseInitState::kMigratedSharedDbOpened);
        return;
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL:
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED:
        if (unique_db_status == Enums::kInvalidOperation) {
          // If unique db does not exist and migration state expects it, reset
          // the migration state since this is not recoverable, and return the
          // shared db. Clear the shared db since it might contain stale data.
          SharedProtoDatabaseClient* client_ptr = client.get();
          client_ptr->UpdateEntriesWithRemoveFilter(
              std::make_unique<KeyValueVector>(),
              base::BindRepeating([](const std::string& key) { return true; }),
              base::BindOnce(&ProtoDatabaseSelector::
                                 InvokeInitUniqueDbMissingSharedCleared,
                             this, std::move(client), std::move(callback)));
          return;
        }
        // If the unique DB failed to open, and the data is located on it then
        // we throw an error.
        std::move(callback).Run(Enums::InitStatus::kError);
        OnInitDone(ProtoDatabaseInitState::kUniqueDbOpenFailed);
        return;
    }
  }

  // Both databases opened correctly. Migrate data and delete old DB if needed.
  if (use_shared_db) {
    switch (client->migration_status()) {
      case SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED:
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL: {
        // Migrate from unique to shared.
        UniqueProtoDatabase* from = unique_db.get();
        UniqueProtoDatabase* to = client.get();
        RecordInitState(ProtoDatabaseInitState::kMigrateToSharedAttempted);
        migration_delegate_->DoMigration(
            from, to,
            base::BindOnce(&ProtoDatabaseSelector::OnMigrationTransferComplete,
                           this, std::move(unique_db), std::move(client),
                           use_shared_db, std::move(callback)));
        return;
      }
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL:
        // Unique db was deleted in previous migration, so nothing to do here.
        return OnMigrationCleanupComplete(std::move(unique_db),
                                          std::move(client), use_shared_db,
                                          std::move(callback), true);
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED:
        // Migration transfer was completed, so just try deleting the unique db.
        return OnMigrationTransferComplete(std::move(unique_db),
                                           std::move(client), use_shared_db,
                                           std::move(callback), true);
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED:
        // Shared db was not deleted in last migration and we want to use shared
        // db. So, delete stale data, and attempt migration.
        return DeleteOldDataAndMigrate(std::move(unique_db), std::move(client),
                                       use_shared_db, std::move(callback));
    }
  } else {
    switch (client->migration_status()) {
      case SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED:
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL: {
        // Migrate from shared to unique.
        UniqueProtoDatabase* from = client.get();
        UniqueProtoDatabase* to = unique_db.get();
        RecordInitState(ProtoDatabaseInitState::kMigrateToUniqueAttempted);
        migration_delegate_->DoMigration(
            from, to,
            base::BindOnce(&ProtoDatabaseSelector::OnMigrationTransferComplete,
                           this, std::move(unique_db), std::move(client),
                           use_shared_db, std::move(callback)));
        return;
      }
      case SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED:
        // Unique db was not deleted in last migration and we want to use unique
        // db. So, delete stale data, and attempt migration.
        return DeleteOldDataAndMigrate(std::move(unique_db), std::move(client),
                                       use_shared_db, std::move(callback));
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL:
        // Shared db was deleted in previous migration, so nothing to do here.
        return OnMigrationCleanupComplete(std::move(unique_db),
                                          std::move(client), use_shared_db,
                                          std::move(callback), true);
      case SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED:
        // Migration transfer was completed, so just try deleting the shared db.
        return OnMigrationTransferComplete(std::move(unique_db),
                                           std::move(client), use_shared_db,
                                           std::move(callback), true);
    }
  }
}

void ProtoDatabaseSelector::DeleteOldDataAndMigrate(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UniqueProtoDatabase* to_remove_old_data =
      use_shared_db ? client.get() : unique_db.get();
  auto maybe_do_migration =
      base::BindOnce(&ProtoDatabaseSelector::MaybeDoMigrationOnDeletingOld,
                     this, std::move(unique_db), std::move(client),
                     std::move(callback), use_shared_db);

  to_remove_old_data->UpdateEntriesWithRemoveFilter(
      std::make_unique<KeyValueVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      std::move(maybe_do_migration));
}

void ProtoDatabaseSelector::MaybeDoMigrationOnDeletingOld(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    Callbacks::InitStatusCallback callback,
    bool use_shared_db,
    bool delete_success) {
  if (!delete_success) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Old data has not been removed from the database we want to use. We also
    // know that previous attempt of migration failed for same reason. Give up
    // on this database and use the other.
    // This update is not necessary since this was the old value. But update to
    // be clear.
    client->UpdateClientInitMetadata(
        use_shared_db
            ? SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED
            : SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);
    db_ = use_shared_db ? std::move(unique_db) : std::move(client);
    std::move(callback).Run(Enums::InitStatus::kOK);
    OnInitDone(ProtoDatabaseInitState::kDeletionOfOldDataFailed);
    return;
  }

  auto* from = use_shared_db ? unique_db.get() : client.get();
  auto* to = use_shared_db ? client.get() : unique_db.get();
  RecordInitState(use_shared_db
                      ? ProtoDatabaseInitState::kMigrateToSharedAttempted
                      : ProtoDatabaseInitState::kMigrateToUniqueAttempted);
  migration_delegate_->DoMigration(
      from, to,
      base::BindOnce(&ProtoDatabaseSelector::OnMigrationTransferComplete, this,
                     std::move(unique_db), std::move(client), use_shared_db,
                     std::move(callback)));
}

void ProtoDatabaseSelector::OnMigrationTransferComplete(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success) {
    // Call Destroy on the DB we no longer want to use.
    auto* db_destroy_ptr = use_shared_db ? unique_db.get() : client.get();
    db_destroy_ptr->Destroy(
        base::BindOnce(&ProtoDatabaseSelector::OnMigrationCleanupComplete, this,
                       std::move(unique_db), std::move(client), use_shared_db,
                       std::move(callback)));
    return;
  }

  // Failing to transfer the old data means that the requested database to be
  // used could have some bad data. So, mark them to be deleted before use in
  // the next runs.
  client->UpdateClientInitMetadata(
      use_shared_db
          ? SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED
          : SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED);
  db_ = use_shared_db ? std::move(unique_db) : std::move(client);
  std::move(callback).Run(Enums::InitStatus::kOK);
  OnInitDone(use_shared_db ? ProtoDatabaseInitState::kMigrateToSharedFailed
                           : ProtoDatabaseInitState::kMigrateToUniqueFailed);
}

void ProtoDatabaseSelector::OnMigrationCleanupComplete(
    std::unique_ptr<UniqueProtoDatabase> unique_db,
    std::unique_ptr<SharedProtoDatabaseClient> client,
    bool use_shared_db,
    Callbacks::InitStatusCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We still return true in our callback below because we do have a database as
  // far as the original caller is concerned. As long as |db_| is assigned, we
  // return true.
  ProtoDatabaseInitState state;
  if (success) {
    client->UpdateClientInitMetadata(
        use_shared_db ? SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL
                      : SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SUCCESSFUL);
    state = use_shared_db ? ProtoDatabaseInitState::kMigrateToSharedSuccess
                          : ProtoDatabaseInitState::kMigrateToUniqueSuccess;
  } else {
    client->UpdateClientInitMetadata(
        use_shared_db
            ? SharedDBMetadataProto::MIGRATE_TO_SHARED_UNIQUE_TO_BE_DELETED
            : SharedDBMetadataProto::MIGRATE_TO_UNIQUE_SHARED_TO_BE_DELETED);
    state =
        use_shared_db
            ? ProtoDatabaseInitState::kMigrateToUniqueCompleteDeletionFailed
            : ProtoDatabaseInitState::kMigrateToSharedCompleteDeletionFailed;
  }
  // Migration transfer was complete. So, we should use the requested database.
  db_ = use_shared_db ? std::move(client) : std::move(unique_db);
  std::move(callback).Run(Enums::InitStatus::kOK);
  OnInitDone(state);
}

void ProtoDatabaseSelector::AddTransaction(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (init_status_) {
    case InitStatus::IN_PROGRESS:
      if (pending_tasks_.size() > 10) {
        std::move(pending_tasks_.front()).Run();
        pending_tasks_.pop();
      }
      pending_tasks_.push(std::move(task));
      break;
    case InitStatus::NOT_STARTED:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case InitStatus::DONE:
      std::move(task).Run();
      break;
  }
}

void ProtoDatabaseSelector::UpdateEntries(
    std::unique_ptr<KeyValueVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_status_, InitStatus::DONE);
  if (!db_) {
    std::move(callback).Run(false);
    return;
  }
  db_->UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                     std::move(callback));
}

void ProtoDatabaseSelector::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false);
    return;
  }
  db_->UpdateEntriesWithRemoveFilter(std::move(entries_to_save),
                                     delete_key_filter, std::move(callback));
}

void ProtoDatabaseSelector::LoadEntries(
    typename Callbacks::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadEntries(std::move(callback));
}

void ProtoDatabaseSelector::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadEntriesWithFilter(key_filter, options, target_prefix,
                             std::move(callback));
}

void ProtoDatabaseSelector::LoadKeysAndEntries(
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeysAndEntries(std::move(callback));
}

void ProtoDatabaseSelector::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeysAndEntriesWithFilter(filter, options, target_prefix,
                                    std::move(callback));
}

void ProtoDatabaseSelector::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeysAndEntriesInRange(start, end, std::move(callback));
}

void ProtoDatabaseSelector::LoadKeysAndEntriesWhile(
    const std::string& start,
    const KeyIteratorController& controller,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeysAndEntriesWhile(start, controller, std::move(callback));
}

void ProtoDatabaseSelector::LoadKeys(Callbacks::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->LoadKeys(std::move(callback));
}

void ProtoDatabaseSelector::GetEntry(const std::string& key,
                                     typename Callbacks::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false, nullptr);
    return;
  }
  db_->GetEntry(key, std::move(callback));
}

void ProtoDatabaseSelector::Destroy(Callbacks::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    if (!unique_database_dir_.empty()) {
      ProtoLevelDBWrapper::Destroy(unique_database_dir_, client_name_,
                                   task_runner_, std::move(callback));
      return;
    }

    std::move(callback).Run(false);
    return;
  }

  db_->Destroy(std::move(callback));
}

void ProtoDatabaseSelector::RemoveKeysForTesting(
    const KeyFilter& key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_) {
    std::move(callback).Run(false);
    return;
  }
  db_->RemoveKeysForTesting(key_filter, target_prefix, std::move(callback));
}

void ProtoDatabaseSelector::InvokeInitUniqueDbMissingSharedCleared(
    std::unique_ptr<SharedProtoDatabaseClient> client,
    Callbacks::InitStatusCallback callback,
    bool shared_cleared) {
  if (!shared_cleared) {
    OnInitDone(
        ProtoDatabaseInitState::kFailureUniqueDbMissingClearSharedFailed);
    std::move(callback).Run(Enums::InitStatus::kError);
    return;
  }
  // Reset state to migrated to shared since unique db is missing.
  client->UpdateClientInitMetadata(
      SharedDBMetadataProto::MIGRATE_TO_SHARED_SUCCESSFUL);
  db_ = std::move(client);
  OnInitDone(ProtoDatabaseInitState::kUniqueDbMissingSharedReturned);
  std::move(callback).Run(Enums::InitStatus::kOK);
}

void ProtoDatabaseSelector::OnInitDone(
    ProtoDatabaseSelector::ProtoDatabaseInitState state) {
  RecordInitState(state);

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_status_ = InitStatus::DONE;
  while (!pending_tasks_.empty()) {
    task_runner_->PostTask(FROM_HERE, std::move(pending_tasks_.front()));
    pending_tasks_.pop();
  }
}

}  // namespace leveldb_proto
