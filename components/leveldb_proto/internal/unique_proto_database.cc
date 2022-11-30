// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/unique_proto_database.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"

namespace leveldb_proto {

UniqueProtoDatabase::UniqueProtoDatabase(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : db_wrapper_(std::make_unique<ProtoLevelDBWrapper>(task_runner)) {}

UniqueProtoDatabase::UniqueProtoDatabase(
    std::unique_ptr<ProtoLevelDBWrapper> db_wrapper)
    : db_wrapper_(std::move(db_wrapper)) {}

UniqueProtoDatabase::UniqueProtoDatabase(
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : UniqueProtoDatabase(task_runner) {
  database_dir_ = database_dir;
  options_ = options;
}

UniqueProtoDatabase::~UniqueProtoDatabase() {
  if (db_.get() &&
      !db_wrapper_->task_runner()->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "Proto database will not be deleted.";
  }
}

void UniqueProtoDatabase::Init(const std::string& client_name,
                               Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_ = std::make_unique<LevelDB>(client_name.c_str());
  db_wrapper_->SetMetricsId(client_name);
  InitWithDatabase(db_.get(), database_dir_, options_, true,
                   std::move(callback));
}

void UniqueProtoDatabase::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    bool destroy_on_corruption,
    Callbacks::InitStatusCallback callback) {
  // We set |destroy_on_corruption| to true to preserve the original behaviour
  // where database corruption led to automatic destruction.
  db_wrapper_->InitWithDatabase(database, database_dir, options,
                                destroy_on_corruption, std::move(callback));
}

void UniqueProtoDatabase::UpdateEntries(
    std::unique_ptr<KeyValueVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    Callbacks::UpdateCallback callback) {
  db_wrapper_->UpdateEntries(std::move(entries_to_save),
                             std::move(keys_to_remove), std::move(callback));
}

void UniqueProtoDatabase::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  db_wrapper_->UpdateEntriesWithRemoveFilter(
      std::move(entries_to_save), delete_key_filter, std::move(callback));
}

void UniqueProtoDatabase::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  db_wrapper_->UpdateEntriesWithRemoveFilter(std::move(entries_to_save),
                                             delete_key_filter, target_prefix,
                                             std::move(callback));
}

void UniqueProtoDatabase::LoadEntries(
    typename Callbacks::LoadCallback callback) {
  db_wrapper_->LoadEntries(std::move(callback));
}

void UniqueProtoDatabase::LoadEntriesWithFilter(
    const KeyFilter& filter,
    typename Callbacks::LoadCallback callback) {
  db_wrapper_->LoadEntriesWithFilter(filter, std::move(callback));
}

void UniqueProtoDatabase::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::LoadCallback callback) {
  db_wrapper_->LoadEntriesWithFilter(key_filter, options, target_prefix,
                                     std::move(callback));
}

void UniqueProtoDatabase::LoadKeysAndEntries(
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->LoadKeysAndEntries(std::move(callback));
}

void UniqueProtoDatabase::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->LoadKeysAndEntriesWithFilter(filter, std::move(callback));
}

void UniqueProtoDatabase::LoadKeysAndEntriesWithFilter(
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->LoadKeysAndEntriesWithFilter(filter, options, target_prefix,
                                            std::move(callback));
}

void UniqueProtoDatabase::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->LoadKeysAndEntriesInRange(start, end, std::move(callback));
}

void UniqueProtoDatabase::LoadKeysAndEntriesWhile(
    const std::string& start,
    const KeyIteratorController& controller,
    typename Callbacks::LoadKeysAndEntriesCallback callback) {
  db_wrapper_->LoadKeysAndEntriesWhile(start, controller, std::move(callback));
}

void UniqueProtoDatabase::LoadKeys(Callbacks::LoadKeysCallback callback) {
  db_wrapper_->LoadKeys(std::move(callback));
}

void UniqueProtoDatabase::LoadKeys(const std::string& target_prefix,
                                   Callbacks::LoadKeysCallback callback) {
  db_wrapper_->LoadKeys(target_prefix, std::move(callback));
}

void UniqueProtoDatabase::GetEntry(const std::string& key,
                                   typename Callbacks::GetCallback callback) {
  db_wrapper_->GetEntry(key, std::move(callback));
}

void UniqueProtoDatabase::Destroy(Callbacks::DestroyCallback callback) {
  db_wrapper_->Destroy(std::move(callback));
}

void UniqueProtoDatabase::RemoveKeysForTesting(
    const KeyFilter& key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  db_wrapper_->RemoveKeys(key_filter, target_prefix, std::move(callback));
}

bool UniqueProtoDatabase::GetApproximateMemoryUse(uint64_t* approx_mem_use) {
  return db_wrapper_->GetApproximateMemoryUse(approx_mem_use);
}

void UniqueProtoDatabase::SetMetricsId(const std::string& id) {
  db_wrapper_->SetMetricsId(id);
}

}  // namespace leveldb_proto
