// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_UNIQUE_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_UNIQUE_PROTO_DATABASE_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

// An implementation of ProtoDatabase<std::string> that manages the lifecycle of
// a unique LevelDB instance.
class COMPONENT_EXPORT(LEVELDB_PROTO) UniqueProtoDatabase {
 public:
  explicit UniqueProtoDatabase(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  explicit UniqueProtoDatabase(std::unique_ptr<ProtoLevelDBWrapper>);
  virtual ~UniqueProtoDatabase();

  UniqueProtoDatabase(
      const base::FilePath& database_dir,
      const leveldb_env::Options& options,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  virtual void Init(const std::string& client_name,
                    Callbacks::InitStatusCallback callback);

  virtual void InitWithDatabase(LevelDB* database,
                                const base::FilePath& database_dir,
                                const leveldb_env::Options& options,
                                bool destroy_on_corruption,
                                Callbacks::InitStatusCallback callback);

  virtual void UpdateEntries(std::unique_ptr<KeyValueVector> entries_to_save,
                             std::unique_ptr<KeyVector> keys_to_remove,
                             Callbacks::UpdateCallback callback);

  virtual void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyValueVector> entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback);
  virtual void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyValueVector> entries_to_save,
      const KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      Callbacks::UpdateCallback callback);

  virtual void LoadEntries(typename Callbacks::LoadCallback callback);

  virtual void LoadEntriesWithFilter(const KeyFilter& filter,
                                     typename Callbacks::LoadCallback callback);
  virtual void LoadEntriesWithFilter(const KeyFilter& key_filter,
                                     const leveldb::ReadOptions& options,
                                     const std::string& target_prefix,
                                     typename Callbacks::LoadCallback callback);

  virtual void LoadKeysAndEntries(
      typename Callbacks::LoadKeysAndEntriesCallback callback);

  virtual void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      typename Callbacks::LoadKeysAndEntriesCallback callback);
  virtual void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      typename Callbacks::LoadKeysAndEntriesCallback callback);
  virtual void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      typename Callbacks::LoadKeysAndEntriesCallback callback);

  virtual void LoadKeys(Callbacks::LoadKeysCallback callback);
  virtual void LoadKeys(const std::string& target_prefix,
                        Callbacks::LoadKeysCallback callback);

  virtual void GetEntry(const std::string& key,
                        typename Callbacks::GetCallback callback);

  virtual void Destroy(Callbacks::DestroyCallback callback);

  void RemoveKeysForTesting(const KeyFilter& key_filter,
                            const std::string& target_prefix,
                            Callbacks::UpdateCallback callback);

  bool GetApproximateMemoryUse(uint64_t* approx_mem_use);

  // Sets the identifier used by the underlying LevelDB wrapper to record
  // metrics.
  void SetMetricsId(const std::string& id);

 protected:
  std::unique_ptr<ProtoLevelDBWrapper> db_wrapper_;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::FilePath database_dir_;
  leveldb_env::Options options_;
  std::unique_ptr<LevelDB> db_;
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_UNIQUE_PROTO_DATABASE_H_
