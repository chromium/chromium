// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_CLIENT_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_CLIENT_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto/shared_db_metadata.pb.h"
#include "components/leveldb_proto/internal/unique_proto_database.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace leveldb_proto {

class SharedProtoDatabase;

// TODO: Move all these as static or member functions in the class.
using ClientCorruptCallback = base::OnceCallback<void(bool)>;
using SharedClientInitCallback =
    base::OnceCallback<void(Enums::InitStatus,
                            SharedDBMetadataProto::MigrationStatus)>;

// An implementation of ProtoDatabase<T> that uses a shared LevelDB and task
// runner.
// Should be created, destroyed, and used on the same sequenced task runner.
class COMPONENT_EXPORT(LEVELDB_PROTO) SharedProtoDatabaseClient
    : public UniqueProtoDatabase {
 public:
  static std::string PrefixForDatabase(ProtoDbType db_type);

  static std::string StripPrefix(const std::string& key,
                                 const std::string& prefix);

  static std::unique_ptr<KeyVector> PrefixStrings(
      std::unique_ptr<KeyVector> strings,
      const std::string& prefix);

  static bool KeyFilterStripPrefix(const KeyFilter& key_filter,
                                   const std::string& prefix,
                                   const std::string& key);

  static void GetSharedDatabaseInitStatusAsync(
      const std::string& client_db_id,
      const scoped_refptr<SharedProtoDatabase>& db,
      Callbacks::InitStatusCallback callback);

  static void UpdateClientMetadataAsync(
      const scoped_refptr<SharedProtoDatabase>& db,
      const std::string& client_db_id,
      SharedDBMetadataProto::MigrationStatus migration_status,
      ClientCorruptCallback callback);

  // Destroys all the data from obsolete clients, for the given |db_wrapper|
  // instance. |callback| is called once all the obsolete clients data are
  // removed, with failure status if one or more of the update fails.
  static void DestroyObsoleteSharedProtoDatabaseClients(
      std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
      Callbacks::UpdateCallback callback);

  // Sets list of client names that are obsolete and will be cleared by next
  // call to DestroyObsoleteSharedProtoDatabaseClients(). |list| is list of dbs
  // with a |LAST| to mark the end of list.
  static void SetObsoleteClientListForTesting(const ProtoDbType* list);

  ~SharedProtoDatabaseClient() override;

  void Init(const std::string& client_uma_name,
            Callbacks::InitStatusCallback callback) override;

  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        bool destroy_on_corruption,
                        Callbacks::InitStatusCallback callback) override;

  // Overrides for prepending namespace and type prefix to all operations on the
  // shared database.
  void UpdateEntries(std::unique_ptr<KeyValueVector> entries_to_save,
                     std::unique_ptr<KeyVector> keys_to_remove,
                     Callbacks::UpdateCallback callback) override;
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyValueVector> entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback) override;
  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyValueVector> entries_to_save,
      const KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      Callbacks::UpdateCallback callback) override;

  void LoadEntries(Callbacks::LoadCallback callback) override;
  void LoadEntriesWithFilter(const KeyFilter& filter,
                             Callbacks::LoadCallback callback) override;
  void LoadEntriesWithFilter(const KeyFilter& key_filter,
                             const leveldb::ReadOptions& options,
                             const std::string& target_prefix,
                             Callbacks::LoadCallback callback) override;

  void LoadKeys(Callbacks::LoadKeysCallback callback) override;
  void LoadKeys(const std::string& target_prefix,
                Callbacks::LoadKeysCallback callback) override;

  void LoadKeysAndEntries(
      Callbacks::LoadKeysAndEntriesCallback callback) override;
  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      Callbacks::LoadKeysAndEntriesCallback callback) override;
  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      Callbacks::LoadKeysAndEntriesCallback callback) override;
  void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      Callbacks::LoadKeysAndEntriesCallback callback) override;

  void GetEntry(const std::string& key,
                Callbacks::GetCallback callback) override;

  void Destroy(Callbacks::DestroyCallback callback) override;

  Callbacks::InitCallback GetInitCallback() const;

  const std::string& client_db_id() const { return prefix_; }

  void set_migration_status(
      SharedDBMetadataProto::MigrationStatus migration_status) {
    migration_status_ = migration_status;
  }

  virtual void UpdateClientInitMetadata(SharedDBMetadataProto::MigrationStatus);

  SharedDBMetadataProto::MigrationStatus migration_status() const {
    return migration_status_;
  }

 private:
  friend class SharedProtoDatabase;
  friend class SharedProtoDatabaseTest;
  friend class SharedProtoDatabaseClientTest;
  friend class TestSharedProtoDatabaseClient;

  // Hide this so clients can only be created by the SharedProtoDatabase.
  SharedProtoDatabaseClient(
      std::unique_ptr<ProtoLevelDBWrapper> db_wrapper,
      ProtoDbType db_type,
      const scoped_refptr<SharedProtoDatabase>& parent_db);

  static void StripPrefixLoadKeysCallback(
      Callbacks::LoadKeysCallback callback,
      const std::string& prefix,
      bool success,
      std::unique_ptr<leveldb_proto::KeyVector> keys);
  static void StripPrefixLoadKeysAndEntriesCallback(
      Callbacks::LoadKeysAndEntriesCallback callback,
      const std::string& prefix,
      bool success,
      std::unique_ptr<KeyValueMap> keys_entries);

  static std::unique_ptr<KeyValueVector> PrefixKeyEntryVector(
      std::unique_ptr<KeyValueVector> kev,
      const std::string& prefix);

  SEQUENCE_CHECKER(sequence_checker_);

  // |is_corrupt_| should be set by the SharedProtoDatabase that creates this
  // when a client is created that doesn't know about a previous shared
  // database corruption.
  bool is_corrupt_ = false;
  SharedDBMetadataProto::MigrationStatus migration_status_ =
      SharedDBMetadataProto::MIGRATION_NOT_ATTEMPTED;

  const std::string prefix_;

  scoped_refptr<SharedProtoDatabase> parent_db_;

  base::WeakPtrFactory<SharedProtoDatabaseClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharedProtoDatabaseClient);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_CLIENT_H_
