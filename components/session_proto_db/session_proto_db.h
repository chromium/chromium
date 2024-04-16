// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_PROTO_DB_SESSION_PROTO_DB_H_
#define COMPONENTS_SESSION_PROTO_DB_SESSION_PROTO_DB_H_

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/commerce/core/proto/persisted_state_db_content.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/session_proto_db/session_proto_storage.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"

namespace {
const char kOrphanedDataCountHistogramName[] =
    "Tabs.PersistedTabData.Storage.LevelDB.OrphanedDataCount";
}  // namespace

class SessionProtoDBTest;

template <typename T>
class SessionProtoDBFactory;

// General purpose per session (BrowserContext/BrowserState), per proto key ->
// proto database where the template is the proto which is being stored. A
// SessionProtoDB should be acquired using SessionProtoDBFactory. SessionProtoDB
// is a wrapper on top of leveldb_proto which:
// - Is specifically for databases which are per session
// (BrowserContext/BrowserState)
//   and per proto (leveldb_proto is a proto database which may or may not be
//   per BrowserContext/BrowserState).
// - Provides a simplified interface for the use cases that surround
//   SessionProtoDB such as providing LoadContentWithPrefix instead of the
//   more generic API in
//   leveldb_proto which requires a filter to be passed in.
// - Is a KeyedService to support the per session (BrowserContext/BrowserState)
//   nature of the database.
template <typename T>
class SessionProtoDB : public KeyedService, public SessionProtoStorage<T> {
 public:
  using KeyAndValue = std::pair<std::string, T>;

  // Callback which is used when content is acquired.
  using LoadCallback = base::OnceCallback<void(bool, std::vector<KeyAndValue>)>;

  // Used for confirming an operation was completed successfully (e.g.
  // insert, delete). This will be invoked on a different SequenceRunner
  // to SessionProtoDB.
  using OperationCallback = base::OnceCallback<void(bool)>;

  // Represents an entry in the database.
  using ContentEntry = typename leveldb_proto::ProtoDatabase<T>::KeyEntryVector;

  // Initializes the database.
  SessionProtoDB(
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& database_dir,
      leveldb_proto::ProtoDbType proto_db_type,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

  SessionProtoDB(const SessionProtoDB&) = delete;
  SessionProtoDB& operator=(const SessionProtoDB&) = delete;
  ~SessionProtoDB() override;

  // SessionProtoStorage implementation:
  void LoadOneEntry(const std::string& key, LoadCallback callback) override;

  void LoadAllEntries(LoadCallback callback) override;

  void LoadContentWithPrefix(const std::string& key_prefix,
                             LoadCallback callback) override;

  void PerformMaintenance(const std::vector<std::string>& keys_to_keep,
                          const std::string& key_substring_to_match,
                          OperationCallback callback) override;

  void InsertContent(const std::string& key,
                     const T& value,
                     OperationCallback callback) override;

  void DeleteOneEntry(const std::string& key,
                      OperationCallback callback) override;

  void UpdateEntries(std::unique_ptr<ContentEntry> entries_to_update,
                     std::unique_ptr<std::vector<std::string>> keys_to_remove,
                     OperationCallback callback) override;

  void DeleteContentWithPrefix(const std::string& key_prefix,
                               OperationCallback callback) override;

  void DeleteAllContent(OperationCallback callback) override;

  void Destroy() const override;

 private:
  friend class ::SessionProtoDBTest;
  template <typename U>
  friend class ::SessionProtoDBFactory;

  // Used for testing.
  SessionProtoDB(
      std::unique_ptr<leveldb_proto::ProtoDatabase<T>> storage_database,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

  // Passes back database status following database initialization.
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);

  // Callback when one entry is loaded.
  void OnLoadOneEntry(LoadCallback callback,
                      bool success,
                      std::unique_ptr<T> entry);

  // Callback when content is loaded.
  void OnLoadContent(LoadCallback callback,
                     bool success,
                     std::unique_ptr<std::vector<T>> content);

  // Callback when PerformMaintenance is complete.
  void OnPerformMaintenance(OperationCallback callback,
                            bool success,
                            std::unique_ptr<std::vector<T>> entries_to_delete);

  // Callback when an operation (e.g. insert or delete) is called.
  void OnOperationCommitted(OperationCallback callback, bool success);

  // Returns true if initialization status of database is not yet known.
  bool InitStatusUnknown() const;

  // Returns true if the database failed to initialize.
  bool FailedToInit() const;

  static bool DatabasePrefixFilter(const std::string& key_prefix,
                                   const std::string& key) {
    return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
  }

  // Status of the database initialization.
  std::optional<leveldb_proto::Enums::InitStatus> database_status_;

  // The database for storing content storage information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<T>> storage_database_;

  // Store operations until the database is initialized at which point
  // |deferred_operations_| is flushed and all operations are executed.
  std::vector<base::OnceClosure> deferred_operations_;

  // Task Runner for posting tasks to UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  base::WeakPtrFactory<SessionProtoDB> weak_ptr_factory_{this};
};

template <typename T>
SessionProtoDB<T>::SessionProtoDB(
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& database_dir,
    leveldb_proto::ProtoDbType proto_db_type,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : SessionProtoStorage<T>(),
      database_status_(std::nullopt),
      storage_database_(proto_database_provider->GetDB<T>(
          proto_db_type,
          database_dir,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}))),
      ui_thread_task_runner_(ui_thread_task_runner) {
  static_assert(std::is_base_of<google::protobuf::MessageLite, T>::value,
                "T must implement 'google::protobuf::MessageLite'");
  storage_database_->Init(base::BindOnce(&SessionProtoDB::OnDatabaseInitialized,
                                         weak_ptr_factory_.GetWeakPtr()));
}

template <typename T>
SessionProtoDB<T>::~SessionProtoDB() = default;

template <typename T>
void SessionProtoDB<T>::LoadOneEntry(const std::string& key,
                                     LoadCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &SessionProtoDB::LoadOneEntry, weak_ptr_factory_.GetWeakPtr(), key,
        std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, std::vector<KeyAndValue>()));
  } else {
    storage_database_->GetEntry(
        key,
        base::BindOnce(&SessionProtoDB::OnLoadOneEntry,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

template <typename T>
void SessionProtoDB<T>::LoadAllEntries(LoadCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(
        base::BindOnce(&SessionProtoDB::LoadAllEntries,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, std::vector<KeyAndValue>()));
  } else {
    storage_database_->LoadEntries(
        base::BindOnce(&SessionProtoDB::OnLoadContent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

template <typename T>
void SessionProtoDB<T>::LoadContentWithPrefix(const std::string& key_prefix,
                                              LoadCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &SessionProtoDB::LoadContentWithPrefix, weak_ptr_factory_.GetWeakPtr(),
        key_prefix, std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false, std::vector<KeyAndValue>()));
  } else {
    storage_database_->LoadEntriesWithFilter(
        base::BindRepeating(&DatabasePrefixFilter, key_prefix),
        {.fill_cache = false},
        /* target_prefix */ "",
        base::BindOnce(&SessionProtoDB::OnLoadContent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

template <typename T>
void SessionProtoDB<T>::PerformMaintenance(
    const std::vector<std::string>& keys_to_keep,
    const std::string& key_substring_to_match,
    OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &SessionProtoDB::PerformMaintenance, weak_ptr_factory_.GetWeakPtr(),
        keys_to_keep, key_substring_to_match, std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  } else {
    // The following could be achieved with UpdateEntriesWithRemoveFilter rather
    // than LoadEntriesWithFilter followed by UpdateEntries, however, that would
    // not allow metrics to be recorded regarding how much orphaned data was
    // identified.
    storage_database_->LoadEntriesWithFilter(
        base::BindRepeating(
            [](const std::vector<std::string>& keys_to_keep,
               const std::string& key_substring_to_match,
               const std::string& key) {
              // Return all keys which where key_substring_to_match is a
              // substring of said keys and hasn't been explicitly marked
              // not to be removed in keys_to_keep.
              return base::Contains(key, key_substring_to_match) &&
                     !base::Contains(keys_to_keep, key);
            },
            keys_to_keep, key_substring_to_match),
        base::BindOnce(&SessionProtoDB::OnPerformMaintenance,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

// Inserts a value for a given key and passes the result (success/failure) to
// OperationCallback.
template <typename T>
void SessionProtoDB<T>::InsertContent(const std::string& key,
                                      const T& value,
                                      OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &SessionProtoDB::InsertContent, weak_ptr_factory_.GetWeakPtr(), key,
        std::move(value), std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  } else {
    auto contents_to_save = std::make_unique<ContentEntry>();
    contents_to_save->emplace_back(key, value);
    storage_database_->UpdateEntries(
        std::move(contents_to_save),
        std::make_unique<std::vector<std::string>>(),
        base::BindOnce(&SessionProtoDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

template <typename T>
void SessionProtoDB<T>::DeleteOneEntry(const std::string& key,
                                       OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &SessionProtoDB::DeleteOneEntry, weak_ptr_factory_.GetWeakPtr(), key,
        std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  } else {
    auto keys = std::make_unique<std::vector<std::string>>();
    keys->push_back(key);
    storage_database_->UpdateEntries(
        std::make_unique<ContentEntry>(), std::move(keys),
        base::BindOnce(&SessionProtoDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

template <typename T>
void SessionProtoDB<T>::UpdateEntries(
    std::unique_ptr<ContentEntry> entries_to_update,
    std::unique_ptr<std::vector<std::string>> keys_to_remove,
    OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &SessionProtoDB::UpdateEntries, weak_ptr_factory_.GetWeakPtr(),
        std::move(entries_to_update), std::move(keys_to_remove),
        std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  } else {
    storage_database_->UpdateEntries(
        std::move(entries_to_update), std::move(keys_to_remove),
        base::BindOnce(&SessionProtoDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

// Deletes content in the database, matching all keys which have a prefix
// that matches the key.
template <typename T>
void SessionProtoDB<T>::DeleteContentWithPrefix(const std::string& key_prefix,
                                                OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(base::BindOnce(
        &SessionProtoDB::DeleteContentWithPrefix,
        weak_ptr_factory_.GetWeakPtr(), key_prefix, std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));

  } else {
    storage_database_->UpdateEntriesWithRemoveFilter(
        std::make_unique<ContentEntry>(),
        base::BindRepeating(&DatabasePrefixFilter, key_prefix),
        base::BindOnce(&SessionProtoDB::OnOperationCommitted,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
}

// Delete all content in the database.
template <typename T>
void SessionProtoDB<T>::DeleteAllContent(OperationCallback callback) {
  if (InitStatusUnknown()) {
    deferred_operations_.push_back(
        base::BindOnce(&SessionProtoDB::DeleteAllContent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else if (FailedToInit()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  } else {
    storage_database_->Destroy(std::move(callback));
  }
}

template <typename T>
void SessionProtoDB<T>::Destroy() const {
  // TODO(davidjm): Consider calling the factory's disassociate method here.
  //                This isn't strictly necessary since it will be called when
  //                the context is destroyed anyway.
}

// Used for tests.
template <typename T>
SessionProtoDB<T>::SessionProtoDB(
    std::unique_ptr<leveldb_proto::ProtoDatabase<T>> storage_database,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : SessionProtoStorage<T>(),
      database_status_(std::nullopt),
      storage_database_(std::move(storage_database)),
      ui_thread_task_runner_(ui_thread_task_runner) {
  static_assert(std::is_base_of<google::protobuf::MessageLite, T>::value,
                "T must implement 'google::protobuf::MessageLite'");
  storage_database_->Init(base::BindOnce(&SessionProtoDB::OnDatabaseInitialized,
                                         weak_ptr_factory_.GetWeakPtr()));
}

// Passes back database status following database initialization.
template <typename T>
void SessionProtoDB<T>::OnDatabaseInitialized(
    leveldb_proto::Enums::InitStatus status) {
  database_status_ =
      std::make_optional<leveldb_proto::Enums::InitStatus>(status);
  for (auto& deferred_operation : deferred_operations_) {
    std::move(deferred_operation).Run();
  }
  deferred_operations_.clear();
}

// Callback when one entry is loaded.
template <typename T>
void SessionProtoDB<T>::OnLoadOneEntry(LoadCallback callback,
                                       bool success,
                                       std::unique_ptr<T> entry) {
  std::vector<KeyAndValue> results;
  if (success && entry) {
    results.emplace_back(entry->key(), *entry);
  }
  std::move(callback).Run(success, std::move(results));
}

// Callback when content is loaded.
template <typename T>
void SessionProtoDB<T>::OnLoadContent(LoadCallback callback,
                                      bool success,
                                      std::unique_ptr<std::vector<T>> content) {
  std::vector<KeyAndValue> results;
  if (success) {
    for (const auto& proto : *content) {
      // TODO(crbug.com/40161040) relax requirement for proto to have a key
      // field and return key value pairs OnLoadContent.
      results.emplace_back(proto.key(), proto);
    }
  }
  std::move(callback).Run(success, std::move(results));
}

template <typename T>
void SessionProtoDB<T>::OnPerformMaintenance(
    OperationCallback callback,
    bool success,
    std::unique_ptr<std::vector<T>> entries_to_delete) {
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();
  if (success) {
    for (const auto& proto : *entries_to_delete) {
      keys_to_delete->emplace_back(proto.key());
    }
    base::UmaHistogramCounts100(kOrphanedDataCountHistogramName,
                                keys_to_delete->size());
  }
  auto save_no_entries =
      std::make_unique<std::vector<std::pair<std::string, T>>>();
  storage_database_->UpdateEntries(
      std::move(save_no_entries), std::move(keys_to_delete),
      base::BindOnce(&SessionProtoDB::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// Callback when an operation (e.g. insert or delete) is called.
template <typename T>
void SessionProtoDB<T>::OnOperationCommitted(OperationCallback callback,
                                             bool success) {
  std::move(callback).Run(success);
}

// Returns true if initialization status of database is not yet known.
template <typename T>
bool SessionProtoDB<T>::InitStatusUnknown() const {
  return database_status_ == std::nullopt;
}

// Returns true if the database failed to initialize.
template <typename T>
bool SessionProtoDB<T>::FailedToInit() const {
  return database_status_.has_value() &&
         database_status_.value() != leveldb_proto::Enums::InitStatus::kOK;
}

#endif  // COMPONENTS_SESSION_PROTO_DB_SESSION_PROTO_DB_H_
