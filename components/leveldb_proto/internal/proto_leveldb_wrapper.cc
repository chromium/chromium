// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_leveldb_wrapper.h"

#include <string>

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/leveldb_proto/internal/leveldb_database.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper_metrics.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace leveldb_proto {

namespace {

Enums::InitStatus InitFromTaskRunner(LevelDB* database,
                                     const base::FilePath& database_dir,
                                     const leveldb_env::Options& options,
                                     bool destroy_on_corruption,
                                     const std::string& client_id) {
  // TODO(cjhopman): Histogram for database size.
  auto status = database->Init(database_dir, options, destroy_on_corruption);
  ProtoLevelDBWrapperMetrics::RecordInit(client_id, status);

  if (status.ok())
    return Enums::InitStatus::kOK;
  if (status.IsCorruption())
    return Enums::InitStatus::kCorrupt;
  if (status.IsNotSupportedError() || status.IsInvalidArgument())
    return Enums::InitStatus::kInvalidOperation;
  return Enums::InitStatus::kError;
}

bool DestroyFromTaskRunner(LevelDB* database, const std::string& client_id) {
  auto status = database->Destroy();
  bool success = status.ok();
  ProtoLevelDBWrapperMetrics::RecordDestroy(client_id, success);

  return success;
}

bool DestroyWithDirectoryFromTaskRunner(const base::FilePath& db_dir,
                                        const std::string& client_id) {
  leveldb::Status result = leveldb::DestroyDB(
      db_dir.AsUTF8Unsafe(), leveldb_proto::CreateSimpleOptions());
  bool success = result.ok();

  if (!client_id.empty())
    ProtoLevelDBWrapperMetrics::RecordDestroy(client_id, success);

  return success;
}

void LoadKeysFromTaskRunner(
    LevelDB* database,
    const std::string& target_prefix,
    const std::string& client_id,
    Callbacks::LoadKeysCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  auto keys = std::make_unique<KeyVector>();
  bool success = database->LoadKeys(target_prefix, keys.get());
  ProtoLevelDBWrapperMetrics::RecordLoadKeys(client_id, success);
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success, std::move(keys)));
}

void RemoveKeysFromTaskRunner(
    LevelDB* database,
    const std::string& target_prefix,
    const KeyFilter& filter,
    const std::string& client_id,
    Callbacks::UpdateCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  leveldb::Status status;
  bool success = database->UpdateWithRemoveFilter(base::StringPairs(), filter,
                                                  target_prefix, &status);
  ProtoLevelDBWrapperMetrics::RecordUpdate(client_id, success, status);
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), success));
}

void RunLoadCallback(Callbacks::LoadCallback callback,
                     bool* success,
                     std::unique_ptr<ValueVector> entries) {
  std::move(callback).Run(*success, std::move(entries));
}

void RunLoadKeysAndEntriesCallback(
    Callbacks::LoadKeysAndEntriesCallback callback,
    bool* success,
    std::unique_ptr<KeyValueMap> keys_entries) {
  std::move(callback).Run(*success, std::move(keys_entries));
}

void RunGetCallback(Callbacks::GetCallback callback,
                    const bool* success,
                    const bool* found,
                    std::unique_ptr<std::string> entry) {
  std::move(callback).Run(*success, *found ? std::move(entry) : nullptr);
}

bool UpdateEntriesFromTaskRunner(
    LevelDB* database,
    std::unique_ptr<KeyValueVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    const std::string& client_id) {
  DCHECK(entries_to_save);
  DCHECK(keys_to_remove);
  leveldb::Status status;
  bool success = database->Save(*entries_to_save, *keys_to_remove, &status);
  ProtoLevelDBWrapperMetrics::RecordUpdate(client_id, success, status);
  return success;
}

bool UpdateEntriesWithRemoveFilterFromTaskRunner(
    LevelDB* database,
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    const std::string& client_id) {
  DCHECK(entries_to_save);
  leveldb::Status status;
  bool success = database->UpdateWithRemoveFilter(
      *entries_to_save, delete_key_filter, target_prefix, &status);
  ProtoLevelDBWrapperMetrics::RecordUpdate(client_id, success, status);
  return success;
}

void LoadKeysAndEntriesFromTaskRunner(LevelDB* database,
                                      const KeyFilter& while_callback,
                                      const KeyFilter& filter,
                                      const leveldb::ReadOptions& options,
                                      const std::string& target_prefix,
                                      const std::string& client_id,
                                      bool* success,
                                      KeyValueMap* keys_entries) {
  DCHECK(success);
  DCHECK(keys_entries);
  keys_entries->clear();

  *success = database->LoadKeysAndEntriesWhile(filter, keys_entries, options,
                                               target_prefix, while_callback);

  ProtoLevelDBWrapperMetrics::RecordLoadKeysAndEntries(client_id, success);
}

void LoadEntriesFromTaskRunner(LevelDB* database,
                               const KeyFilter& filter,
                               const leveldb::ReadOptions& options,
                               const std::string& target_prefix,
                               const std::string& client_id,
                               bool* success,
                               ValueVector* entries) {
  *success = database->LoadWithFilter(filter, entries, options, target_prefix);

  ProtoLevelDBWrapperMetrics::RecordLoadEntries(client_id, success);
}

void GetEntryFromTaskRunner(LevelDB* database,
                            const std::string& key,
                            const std::string& client_id,
                            bool* success,
                            bool* found,
                            std::string* entry) {
  leveldb::Status status;
  *success = database->Get(key, found, entry, &status);

  ProtoLevelDBWrapperMetrics::RecordGet(client_id, *success, *found, status);
}
}  // namespace

ProtoLevelDBWrapper::ProtoLevelDBWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoLevelDBWrapper::ProtoLevelDBWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    LevelDB* db)
    : task_runner_(task_runner), db_(db) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoLevelDBWrapper::~ProtoLevelDBWrapper() = default;

void ProtoLevelDBWrapper::RunInitCallback(Callbacks::InitCallback callback,
                                          const leveldb::Status* status) {
  std::move(callback).Run(status->ok());
}

void ProtoLevelDBWrapper::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    bool destroy_on_corruption,
    Callbacks::InitStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(database);
  db_ = database;

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(InitFromTaskRunner, base::Unretained(db_), database_dir,
                     options, destroy_on_corruption, metrics_id_),
      std::move(callback));
}

void ProtoLevelDBWrapper::UpdateEntries(
    std::unique_ptr<KeyValueVector> entries_to_save,
    std::unique_ptr<KeyVector> keys_to_remove,
    typename Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(UpdateEntriesFromTaskRunner, base::Unretained(db_),
                     std::move(entries_to_save), std::move(keys_to_remove),
                     metrics_id_),
      std::move(callback));
}

void ProtoLevelDBWrapper::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateEntriesWithRemoveFilter(std::move(entries_to_save), delete_key_filter,
                                std::string(), std::move(callback));
}

void ProtoLevelDBWrapper::UpdateEntriesWithRemoveFilter(
    std::unique_ptr<KeyValueVector> entries_to_save,
    const KeyFilter& delete_key_filter,
    const std::string& target_prefix,
    Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(UpdateEntriesWithRemoveFilterFromTaskRunner,
                     base::Unretained(db_), std::move(entries_to_save),
                     delete_key_filter, target_prefix, metrics_id_),
      std::move(callback));
}

void ProtoLevelDBWrapper::LoadEntries(Callbacks::LoadCallback callback) {
  LoadEntriesWithFilter(KeyFilter(), std::move(callback));
}

void ProtoLevelDBWrapper::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    Callbacks::LoadCallback callback) {
  LoadEntriesWithFilter(key_filter, leveldb::ReadOptions(), std::string(),
                        std::move(callback));
}

void ProtoLevelDBWrapper::LoadEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    Callbacks::LoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool* success = new bool(false);
  auto entries = std::make_unique<ValueVector>();
  // Get this pointer before |entries| is std::move()'d so we can use it below.
  auto* entries_ptr = entries.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(LoadEntriesFromTaskRunner, base::Unretained(db_),
                     key_filter, options, target_prefix, metrics_id_, success,
                     entries_ptr),
      base::BindOnce(RunLoadCallback, std::move(callback), base::Owned(success),
                     std::move(entries)));
}

void ProtoLevelDBWrapper::LoadKeysAndEntries(
    Callbacks::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(KeyFilter(), std::move(callback));
}

void ProtoLevelDBWrapper::LoadKeysAndEntriesWithFilter(
    const KeyFilter& key_filter,
    Callbacks::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWithFilter(key_filter, leveldb::ReadOptions(),
                               std::string(), std::move(callback));
}

void ProtoLevelDBWrapper::LoadKeysAndEntriesWithFilter(
    const KeyFilter& key_filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    Callbacks::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWhile(
      base::BindRepeating(
          [](const std::string& prefix, const std::string& key) {
            return base::StartsWith(key, prefix, base::CompareCase::SENSITIVE);
          },
          target_prefix),
      key_filter, options, target_prefix, std::move(callback));
}

void ProtoLevelDBWrapper::LoadKeysAndEntriesInRange(
    const std::string& start,
    const std::string& end,
    Callbacks::LoadKeysAndEntriesCallback callback) {
  LoadKeysAndEntriesWhile(
      base::BindRepeating(
          [](const std::string& range_end, const std::string& key) {
            return key.compare(range_end) <= 0;
          },
          end),
      KeyFilter(), leveldb::ReadOptions(), start, std::move(callback));
}

void ProtoLevelDBWrapper::LoadKeysAndEntriesWhile(
    const KeyFilter& while_callback,
    const KeyFilter& filter,
    const leveldb::ReadOptions& options,
    const std::string& target_prefix,
    Callbacks::LoadKeysAndEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool* success = new bool(false);
  auto keys_entries = std::make_unique<KeyValueMap>();
  // Get this pointer before |keys_entries| is std::move()'d so we can use it
  // below.
  auto* keys_entries_ptr = keys_entries.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(LoadKeysAndEntriesFromTaskRunner, base::Unretained(db_),
                     while_callback, filter, options, target_prefix,
                     metrics_id_, success, keys_entries_ptr),
      base::BindOnce(RunLoadKeysAndEntriesCallback, std::move(callback),
                     base::Owned(success), std::move(keys_entries)));
}

void ProtoLevelDBWrapper::GetEntry(const std::string& key,
                                   Callbacks::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool* success = new bool(false);
  bool* found = new bool(false);
  auto entry = std::make_unique<std::string>();
  // Get this pointer before |entry| is std::move()'d so we can use it below.
  auto* entry_ptr = entry.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(GetEntryFromTaskRunner, base::Unretained(db_), key,
                     metrics_id_, success, found, entry_ptr),
      base::BindOnce(RunGetCallback, std::move(callback), base::Owned(success),
                     base::Owned(found), std::move(entry)));
}

void ProtoLevelDBWrapper::LoadKeys(
    typename Callbacks::LoadKeysCallback callback) {
  LoadKeys(std::string(), std::move(callback));
}

void ProtoLevelDBWrapper::LoadKeys(
    const std::string& target_prefix,
    typename Callbacks::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(LoadKeysFromTaskRunner, base::Unretained(db_),
                                target_prefix, metrics_id_, std::move(callback),
                                base::SequencedTaskRunnerHandle::Get()));
}

void ProtoLevelDBWrapper::RemoveKeys(const KeyFilter& filter,
                                     const std::string& target_prefix,
                                     Callbacks::UpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(RemoveKeysFromTaskRunner, base::Unretained(db_),
                     target_prefix, filter, metrics_id_, std::move(callback),
                     base::SequencedTaskRunnerHandle::Get()));
}

void ProtoLevelDBWrapper::Destroy(Callbacks::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(DestroyFromTaskRunner, base::Unretained(db_), metrics_id_),
      std::move(callback));
}

void ProtoLevelDBWrapper::Destroy(
    const base::FilePath& db_dir,
    const std::string& client_id,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    Callbacks::DestroyCallback callback) {
  base::PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::BindOnce(DestroyWithDirectoryFromTaskRunner, db_dir, client_id),
      std::move(callback));
}

void ProtoLevelDBWrapper::SetMetricsId(const std::string& id) {
  metrics_id_ = id;
}

bool ProtoLevelDBWrapper::GetApproximateMemoryUse(uint64_t* approx_mem_use) {
  if (!db_)
    return 0;

  return db_->GetApproximateMemoryUse(approx_mem_use);
}

const scoped_refptr<base::SequencedTaskRunner>&
ProtoLevelDBWrapper::task_runner() {
  return task_runner_;
}

}  // namespace leveldb_proto
