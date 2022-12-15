// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_IMPL_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_sync_bridge.h"

namespace base {
class Clock;
}  // namespace base

namespace syncer {
class ModelTypeChangeProcessor;
}  // namespace syncer

// Concrete implementation of a reading list model using in memory lists.
class ReadingListModelImpl : public ReadingListModel {
 public:
  // Initialize a ReadingListModelImpl to load and save data in
  // |storage_layer|, which must not be null.
  // |clock| will be used to timestamp all the operations.
  ReadingListModelImpl(std::unique_ptr<ReadingListModelStorage> storage_layer,
                       base::Clock* clock);
  ~ReadingListModelImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // ReadingListModel implementation.
  bool loaded() const override;
  bool IsPerformingBatchUpdates() const override;
  ReadingListSyncBridge* GetModelTypeSyncBridge() override;
  std::unique_ptr<ScopedReadingListBatchUpdate> BeginBatchUpdates() override;
  base::flat_set<GURL> GetKeys() const override;
  size_t size() const override;
  size_t unread_size() const override;
  size_t unseen_size() const override;
  void MarkAllSeen() override;
  bool DeleteAllEntries() override;
  const ReadingListEntry* GetEntryByURL(const GURL& gurl) const override;
  bool IsUrlSupported(const GURL& url) override;
  const ReadingListEntry& AddOrReplaceEntry(
      const GURL& url,
      const std::string& title,
      reading_list::EntrySource source,
      base::TimeDelta estimated_read_time) override;
  void RemoveEntryByURL(const GURL& url) override;
  void SetReadStatusIfExists(const GURL& url, bool read) override;
  void SetEntryTitleIfExists(const GURL& url,
                             const std::string& title) override;
  void SetEstimatedReadTimeIfExists(
      const GURL& url,
      base::TimeDelta estimated_read_time) override;
  void SetEntryDistilledStateIfExists(
      const GURL& url,
      ReadingListEntry::DistillationState state) override;
  void SetEntryDistilledInfoIfExists(const GURL& url,
                                     const base::FilePath& distilled_path,
                                     const GURL& distilled_url,
                                     int64_t distilation_size,
                                     base::Time distilation_time) override;
  void AddObserver(ReadingListModelObserver* observer) override;
  void RemoveObserver(ReadingListModelObserver* observer) override;

  // API specifically for changes received via sync.
  void SyncAddEntry(std::unique_ptr<ReadingListEntry> entry);
  ReadingListEntry* SyncMergeEntry(std::unique_ptr<ReadingListEntry> entry);
  void SyncRemoveEntry(const GURL& url);

  class ScopedReadingListBatchUpdateImpl : public ScopedReadingListBatchUpdate,
                                           public ReadingListModelObserver {
   public:
    explicit ScopedReadingListBatchUpdateImpl(ReadingListModelImpl* model);
    ~ScopedReadingListBatchUpdateImpl() override;

    syncer::MetadataChangeList* GetSyncMetadataChangeList();

    // ReadingListModelObserver overrides.
    void ReadingListModelLoaded(const ReadingListModel* model) override;
    void ReadingListModelBeingShutdown(const ReadingListModel* model) override;

   private:
    raw_ptr<ReadingListModelImpl> model_;
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> storage_token_;
  };

  // Same as BeginBatchUpdates(), but returns specifically
  // ReadingListModelImpl's ScopedReadingListBatchUpdateImpl.
  std::unique_ptr<ScopedReadingListBatchUpdateImpl>
  BeginBatchUpdatesWithSyncMetadata();

  // Test-only factory function to inject an arbitrary change processor.
  static std::unique_ptr<ReadingListModelImpl> BuildNewForTest(
      std::unique_ptr<ReadingListModelStorage> storage_layer,
      base::Clock* clock,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

 private:
  ReadingListModelImpl(
      std::unique_ptr<ReadingListModelStorage> storage_layer,
      base::Clock* clock,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

  void StoreLoaded(ReadingListModelStorage::LoadResultOrError result_or_error);

  // Tells model that batch updates have completed. Called from
  // ScopedReadingListBatchUpdateImpl's destructor.
  void EndBatchUpdates();

  // Returns a mutable pointer to the entry with URL |url|. Return nullptr if
  // no entry is found.
  ReadingListEntry* GetMutableEntryFromURL(const GURL& url);

  // Returns the |storage_layer_| of the model.
  ReadingListModelStorage* StorageLayer();

  // Add |entry| to the model, which must not exist before, and notify the sync
  // bridge if |source| is not ADDED_VIA_SYNC.
  void AddEntryImpl(std::unique_ptr<ReadingListEntry> entry,
                    reading_list::EntrySource source);

  // Remove entry |url| and propagate to the sync bridge if |from_sync| is
  // false.
  void RemoveEntryByURLImpl(const GURL& url, bool from_sync);

  // Update the 3 counts above considering addition/removal of |entry|.
  void UpdateEntryStateCountersOnEntryRemoval(const ReadingListEntry& entry);
  void UpdateEntryStateCountersOnEntryInsertion(const ReadingListEntry& entry);

  const std::unique_ptr<ReadingListModelStorage> storage_layer_;
  const raw_ptr<base::Clock> clock_;

  ReadingListSyncBridge sync_bridge_;

  base::ObserverList<ReadingListModelObserver>::Unchecked observers_;

  bool loaded_ = false;

  std::map<GURL, ReadingListEntry> entries_;
  size_t unread_entry_count_ = 0;
  size_t read_entry_count_ = 0;
  size_t unseen_entry_count_ = 0;

  unsigned int current_batch_updates_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ReadingListModelImpl> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_IMPL_H_
