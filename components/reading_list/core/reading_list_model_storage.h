// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/sync/model/metadata_batch.h"

class GURL;

namespace base {
class Clock;
}  // namespace base

namespace syncer {
class MetadataChangeList;
}  // namespace syncer

// Interface for a persistence layer for reading list.
// All interface methods have to be called on main thread.
class ReadingListModelStorage {
 public:
  using ReadingListEntries = std::map<GURL, scoped_refptr<ReadingListEntry>>;
  using LoadResult =
      std::pair<ReadingListEntries, std::unique_ptr<syncer::MetadataBatch>>;
  using LoadResultOrError = base::expected<LoadResult, std::string>;
  using LoadCallback = base::OnceCallback<void(LoadResultOrError)>;

  class ScopedBatchUpdate;

  ReadingListModelStorage() = default;

  ReadingListModelStorage(const ReadingListModelStorage&) = delete;
  ReadingListModelStorage& operator=(const ReadingListModelStorage&) = delete;

  virtual ~ReadingListModelStorage() = default;

  // Triggers store initialization and loading of persistent entries. Must be
  // called no more than once. Upon completion, |load_cb| is invoked.
  virtual void Load(base::Clock* clock, LoadCallback load_cb) = 0;

  // Starts a transaction. All Save/Remove entry will be delayed until the
  // transaction is commited.
  // Multiple transaction can be started at the same time. Commit will happen
  // when the last transaction is commited.
  // Returns a scoped batch update object that should be retained while the
  // batch update is performed. Deallocating this object will inform model that
  // the batch update has completed.
  virtual std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() = 0;

  // A robust way to bulk delete all data and sync metadata in storage. The same
  // behavior could be theoretically achieved by deleting entries individually,
  // but this specialized API is just more robust.
  virtual void DeleteAllEntriesAndSyncMetadata() = 0;

  class ScopedBatchUpdate {
   public:
    ScopedBatchUpdate() = default;

    ScopedBatchUpdate(const ScopedBatchUpdate&) = delete;
    ScopedBatchUpdate& operator=(const ScopedBatchUpdate&) = delete;

    virtual ~ScopedBatchUpdate() = default;

    // Saves or updates an entry. If the entry is not yet in the database, it is
    // created.
    virtual void SaveEntry(const ReadingListEntry& entry) = 0;

    // Removed an entry from the storage.
    virtual void RemoveEntry(const GURL& entry_url) = 0;

    // Allows modifications to sync metadata in storage.
    virtual syncer::MetadataChangeList* GetSyncMetadataChangeList() = 0;
  };
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_
