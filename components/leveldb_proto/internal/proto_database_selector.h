// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_SELECTOR_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_SELECTOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace leveldb_proto {

class MigrationDelegate;
class SharedProtoDatabase;
class SharedProtoDatabaseClient;
class SharedProtoDatabaseProvider;
class UniqueProtoDatabase;

// A wrapper around unique and shared database client. Handles initialization of
// underlying database as unique or shared as requested.
// TODO: Discuss the init flow/migration path for unique/shared DB here.
class COMPONENT_EXPORT(LEVELDB_PROTO) ProtoDatabaseSelector
    : public base::RefCountedThreadSafe<ProtoDatabaseSelector> {
 public:
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "ProtoDatabaseInitState" in src/tools/metrics/histograms/enums.xml.
  enum class ProtoDatabaseInitState {
    kSharedDbInitAttempted = 0,
    kFailureUniqueDbCorrupted = 1,
    kFailureNoDatabaseProvider = 2,  // Deprecated.
    kBothUniqueAndSharedFailedOpen = 3,
    kSharedDbClientMissingInitFailed = 4,
    kSharedDbClientMissingUniqueReturned = 5,
    kSharedDbOpenFailed = 6,
    kUniqueDbMissingSharedReturned = 7,
    kUniqueDbOpenFailed = 8,
    kMigrateToSharedAttempted = 9,
    kMigrateToUniqueAttempted = 10,
    kMigratedSharedDbOpened = 11,
    kDeletionOfOldDataFailed = 12,
    kMigrateToSharedFailed = 13,
    kMigrateToUniqueFailed = 14,
    kMigrateToSharedCompleteDeletionFailed = 15,
    kMigrateToUniqueCompleteDeletionFailed = 16,
    kMigrateToSharedSuccess = 17,
    kMigrateToUniqueSuccess = 18,
    kLegacyInitCalled = 19,
    kSharedDbMetadataLoadFailed = 20,
    kSharedDbMetadataWriteFailed = 21,
    kSharedDbClientCorrupt = 22,
    kSharedDbClientSuccess = 23,
    kSharedLevelDbInitFailure = 24,
    kSharedDbClientMissing = 25,
    kFailureNoSharedDBProviderUniqueFailed = 26,
    kSuccessNoSharedDBProviderUniqueSucceeded = 27,
    kMaxValue = kSuccessNoSharedDBProviderUniqueSucceeded,
  };

  static void RecordInitState(ProtoDatabaseInitState state);

  ProtoDatabaseSelector(
      ProtoDbType db_type,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<SharedProtoDatabaseProvider> db_provider);

  void InitWithDatabase(
      LevelDB* database,
      const base::FilePath& database_dir,
      const leveldb_env::Options& options,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      Callbacks::InitStatusCallback callback);

  void InitUniqueOrShared(
      const std::string& client_name,
      base::FilePath db_dir,
      const leveldb_env::Options& unique_db_options,
      bool use_shared_db,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      Callbacks::InitStatusCallback callback);

  void AddTransaction(base::OnceClosure task);

  // DO NOT USE any of the functions below directly. They should be posted as
  // transaction tasks using AddTransaction().
  void UpdateEntries(std::unique_ptr<KeyValueVector> entries_to_save,
                     std::unique_ptr<KeyVector> keys_to_remove,
                     Callbacks::UpdateCallback callback);

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyValueVector> entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback);

  void LoadEntries(typename Callbacks::LoadCallback callback);

  void LoadEntriesWithFilter(const KeyFilter& key_filter,
                             const leveldb::ReadOptions& options,
                             const std::string& target_prefix,
                             typename Callbacks::LoadCallback callback);

  void LoadKeysAndEntries(
      typename Callbacks::LoadKeysAndEntriesCallback callback);

  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::LoadKeysAndEntriesCallback callback);
  void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      typename Callbacks::LoadKeysAndEntriesCallback callback);

  void LoadKeys(Callbacks::LoadKeysCallback callback);

  void GetEntry(const std::string& key,
                typename Callbacks::GetCallback callback);

  void Destroy(Callbacks::DestroyCallback callback);

  void RemoveKeysForTesting(const KeyFilter& key_filter,
                            const std::string& target_prefix,
                            Callbacks::UpdateCallback callback);

  UniqueProtoDatabase* db_for_testing() { return db_.get(); }

 private:
  friend class base::RefCountedThreadSafe<ProtoDatabaseSelector>;
  template <typename T>
  friend class ProtoDatabaseImplTest;

  enum class InitStatus {
    NOT_STARTED,
    IN_PROGRESS,
    DONE  // success or failure.
  };

  ~ProtoDatabaseSelector();

  void OnInitUniqueDB(std::unique_ptr<UniqueProtoDatabase> db,
                      bool use_shared_db,
                      Callbacks::InitStatusCallback callback,
                      Enums::InitStatus status);

  void OnInitSharedDB(std::unique_ptr<UniqueProtoDatabase> unique_db,
                      Enums::InitStatus unique_db_status,
                      bool use_shared_db,
                      Callbacks::InitStatusCallback callback,
                      scoped_refptr<SharedProtoDatabase> shared_db);
  void OnGetSharedDBClient(std::unique_ptr<UniqueProtoDatabase> unique_db,
                           Enums::InitStatus unique_db_status,
                           bool use_shared_db,
                           Callbacks::InitStatusCallback callback,
                           std::unique_ptr<SharedProtoDatabaseClient> client,
                           Enums::InitStatus shared_db_status);
  void DeleteOldDataAndMigrate(
      std::unique_ptr<UniqueProtoDatabase> unique_db,
      std::unique_ptr<SharedProtoDatabaseClient> client,
      bool use_shared_db,
      Callbacks::InitStatusCallback callback);
  void MaybeDoMigrationOnDeletingOld(
      std::unique_ptr<UniqueProtoDatabase> unique_db,
      std::unique_ptr<SharedProtoDatabaseClient> client,
      Callbacks::InitStatusCallback init_callback,
      bool use_shared_db,
      bool delete_success);
  void OnMigrationTransferComplete(
      std::unique_ptr<UniqueProtoDatabase> unique_db,
      std::unique_ptr<SharedProtoDatabaseClient> client,
      bool use_shared_db,
      Callbacks::InitStatusCallback callback,
      bool success);
  void OnMigrationCleanupComplete(
      std::unique_ptr<UniqueProtoDatabase> unique_db,
      std::unique_ptr<SharedProtoDatabaseClient> client,
      bool use_shared_db,
      Callbacks::InitStatusCallback callback,
      bool success);
  void OnInitDone(ProtoDatabaseInitState state);

  ProtoDbType db_type_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const std::unique_ptr<SharedProtoDatabaseProvider> db_provider_;
  const std::unique_ptr<MigrationDelegate> migration_delegate_;

  InitStatus init_status_ = InitStatus::NOT_STARTED;
  base::queue<base::OnceClosure> pending_tasks_;
  std::unique_ptr<UniqueProtoDatabase> db_;
  base::FilePath unique_database_dir_;
  std::string client_name_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_DATABASE_SELECTOR_H_
