// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_

#include <memory>

#include "components/reading_list/core/reading_list_entry.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_sync_bridge.h"

class ReadingListModel;
class ReadingListStoreDelegate;

namespace base {
class Clock;
}

namespace syncer {
class ModelTypeChangeProcessor;
}

// Interface for a persistence layer for reading list.
// All interface methods have to be called on main thread.
class ReadingListModelStorage : public syncer::ModelTypeSyncBridge {
 public:
  class ScopedBatchUpdate;

  ReadingListModelStorage(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

  ReadingListModelStorage(const ReadingListModelStorage&) = delete;
  ReadingListModelStorage& operator=(const ReadingListModelStorage&) = delete;

  ~ReadingListModelStorage() override;

  // Sets the model the Storage is backing.
  // This will trigger store initalization and load persistent entries.
  // Pass the |clock| from the |model| to ensure synchroization when loading
  // entries. Must be called no more than once.
  virtual void SetReadingListModel(ReadingListModel* model,
                                   ReadingListStoreDelegate* delegate,
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

  class ScopedBatchUpdate {
   public:
    ScopedBatchUpdate() {}

    ScopedBatchUpdate(const ScopedBatchUpdate&) = delete;
    ScopedBatchUpdate& operator=(const ScopedBatchUpdate&) = delete;

    virtual ~ScopedBatchUpdate() {}
  };
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_H_
