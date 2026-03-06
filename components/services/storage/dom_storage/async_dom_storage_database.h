// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/threading/sequence_bound.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

class AsyncDomStorageDatabase {
 public:
  using StatusCallback = base::OnceCallback<void(DbStatus)>;

  AsyncDomStorageDatabase(const AsyncDomStorageDatabase&) = delete;
  AsyncDomStorageDatabase& operator=(const AsyncDomStorageDatabase&) = delete;

  ~AsyncDomStorageDatabase();

  // Creates an `AsyncDomStorageDatabase` then asynchronously opens the
  // database. Callers must wait to use `AsyncDomStorageDatabase` until
  // `callback` completes with an OK status. After failing to open,
  // `AsyncDomStorageDatabase` must be discarded.
  //
  // To create an in-memory database, provide an empty `database_path`.
  static std::unique_ptr<AsyncDomStorageDatabase> Open(
      StorageType storage_type,
      const base::FilePath& database_path,
      const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
          memory_dump_id,
      StatusCallback callback);

  // An interface that represents a source of commits. Practically speaking,
  // this is a `StorageAreaImpl`.
  class Committer {
   public:
    virtual std::optional<DomStorageDatabase::MapBatchUpdate>
    CollectCommit() = 0;
    virtual base::OnceCallback<void(DbStatus)> GetCommitCompleteCallback() = 0;
  };

  base::SequenceBound<DomStorageDatabase>& database() { return database_; }

  // The functions below use `base::SequenceBound` to read and write
  // `database_` through the `DomStorageDatabase` interface. See function
  // comments in `dom_storage_database.h` for more details.
  using ReadMapKeyValuesCallback = base::OnceCallback<void(
      StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>)>;
  void ReadMapKeyValues(DomStorageDatabase::MapLocator map_locator,
                        ReadMapKeyValuesCallback callback);
  void CloneMap(DomStorageDatabase::MapLocator source_map,
                DomStorageDatabase::MapLocator target_map,
                StatusCallback callback);

  using ReadAllMetadataCallback =
      base::OnceCallback<void(StatusOr<DomStorageDatabase::Metadata>)>;
  void ReadAllMetadata(ReadAllMetadataCallback callback);

  void PutMetadata(DomStorageDatabase::Metadata metadata,
                   StatusCallback callback);
  void DeleteStorageKeysFromSession(
      std::string session_id,
      std::vector<blink::StorageKey> metadata_to_delete,
      std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
      StatusCallback callback);
  void DeleteSessions(
      std::vector<std::string> session_ids,
      std::vector<DomStorageDatabase::MapLocator> maps_to_delete,
      StatusCallback callback);
  void PurgeOriginsForShutdown(std::set<url::Origin> origins);
  void CleanUpStaleData(StatusCallback callback);

  // Registers or unregisters `source` such that its commits will be batched
  // with other registered committers.
  void AddCommitter(Committer* source);
  void RemoveCommitter(Committer* source);

  // To be called by a committer when it has data that should be committed
  // without delay. Persists the list of pending `Commit` batches from
  // `committers_` using `RunDatabaseTask()`. After the database task, runs the
  // completed callback for each `Committer` that provided a `Commit`.
  void InitiateCommit();

 private:
  AsyncDomStorageDatabase(StorageType storage_type, bool in_memory);

  std::string_view StorageTypeForHistograms() const;
  std::string GetHistogram(std::string_view operation) const;

  // Sets `is_database_opened_` to true when `open_status` is ok.  Then runs
  // `callback` with `open_status`.
  void OnDatabaseOpened(StatusCallback callback, DbStatus open_status);

  // `database_` must not be used until `is_database_opened_` is true.
  bool is_database_opened_ = false;
  base::SequenceBound<DomStorageDatabase> database_;

  std::set<raw_ptr<Committer>> committers_;

  const StorageType storage_type_;
  const bool in_memory_;

  base::WeakPtrFactory<AsyncDomStorageDatabase> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_ASYNC_DOM_STORAGE_DATABASE_H_
