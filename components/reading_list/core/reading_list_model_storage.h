// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_

#include <memory>

#include "components/reading_list/core/reading_list_entry.h"

class ReadingListModel;
class ReadingListSyncBridgeDelegate;

namespace base {
class Clock;
}

namespace syncer {
class ModelTypeSyncBridge;
}

// Interface for a persistence layer for reading list.
// All interface methods have to be called on main thread.
class ReadingListModelStorage {
 public:
  class ScopedBatchUpdate;

  ReadingListModelStorage() = default;

  ReadingListModelStorage(const ReadingListModelStorage&) = delete;
  ReadingListModelStorage& operator=(const ReadingListModelStorage&) = delete;

  virtual ~ReadingListModelStorage() = default;

  // Sets the model the Storage is backing.
  // This will trigger store initalization and load persistent entries.
  // Pass the |clock| from the |model| to ensure synchroization when loading
  // entries. Must be called no more than once.
  // TODO(crbug.com/1386158): ReadingListSyncBridgeDelegate shouldn't belong in
  // this interface.
  virtual void SetReadingListModel(ReadingListModel* model,
                                   ReadingListSyncBridgeDelegate* delegate,
                                   base::Clock* clock) = 0;

  // Starts a transaction. All Save/Remove entry will be delayed until the
  // transaction is commited.
  // Multiple transaction can be started at the same time. Commit will happen
  // when the last transaction is commited.
  // Returns a scoped batch update object that should be retained while the
  // batch update is performed. Deallocating this object will inform model that
  // the batch update has completed.
  virtual std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() = 0;

  // Saves or updates an entry. If the entry is not yet in the database, it is
  // created.
  virtual void SaveEntry(const ReadingListEntry& entry) = 0;

  // Removed an entry from the storage.
  virtual void RemoveEntry(const ReadingListEntry& entry) = 0;

  // Returns the ModelTypeSyncBridge responsible for handling sync message.
  // TODO(crbug.com/1386158): This shouldn't belong in this interface.
  virtual syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() = 0;

  class ScopedBatchUpdate {
   public:
    ScopedBatchUpdate() {}

    ScopedBatchUpdate(const ScopedBatchUpdate&) = delete;
    ScopedBatchUpdate& operator=(const ScopedBatchUpdate&) = delete;

    virtual ~ScopedBatchUpdate() {}
  };
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_
