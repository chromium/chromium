// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_IMPL_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_IMPL_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "components/reading_list/core/reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_sync_bridge.h"
#include "components/sync/base/storage_type.h"
#include "google_apis/gaia/core_account_id.h"

namespace base {
class Clock;
}  // namespace base

namespace syncer {
class DataTypeLocalChangeProcessor;
}  // namespace syncer

// Concrete implementation of a reading list model using in memory lists.
class ReadingListModelImpl : public ReadingListModel {
 public:
  // Initialize a ReadingListModelImpl to load and save data in |storage_layer|,
  // which must not be null. |sync_storage_type_for_uma| specifies whether the
  // model is meant to sync in transport-mode or the default and traditional
  // unspecified mode, for the purpose of metric-reporting.
  // |wipe_model_upon_sync_disabled_behavior| influences what happens when sync
  // is disabled. |clock| will be used to timestamp all the operations.
  ReadingListModelImpl(std::unique_ptr<ReadingListModelStorage> storage_layer,
                       syncer::StorageType sync_storage_type_for_uma,
                       syncer::WipeModelUponSyncDisabledBehavior
                           wipe_model_upon_sync_disabled_behavior,
                       base::Clock* clock);
  ~ReadingListModelImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // ReadingListModel implementation.
  bool loaded() const override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate()
      override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegateForTransportMode() override;
  bool IsPerformingBatchUpdates() const override;
  std::unique_ptr<ScopedReadingListBatchUpdate> BeginBatchUpdates() override;
  base::flat_set<GURL> GetKeys() const override;
  size_t size() const override;
  size_t unread_size() const override;
  size_t unseen_size() const override;
  void MarkAllSeen() override;
  bool DeleteAllEntries(const base::Location& location) override;
  scoped_refptr<const ReadingListEntry> GetEntryByURL(
      const GURL& gurl) const override;
  bool IsUrlSupported(const GURL& url) override;
  CoreAccountId GetAccountWhereEntryIsSavedTo(const GURL& url) override;
  bool NeedsExplicitUploadToSyncServer(const GURL& url) const override;
  void MarkAllForUploadToSyncServerIfNeeded() override;
  const ReadingListEntry& AddOrReplaceEntry(
      const GURL& url,
      const std::string& title,
      reading_list::EntrySource source,
      base::TimeDelta estimated_read_time) override;
  void RemoveEntryByURL(const GURL& url,
                        const base::Location& location) override;
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
  void RecordCountMetricsOnUMAUpload() const override;

  // Add |entry| to the model, which must not exist before, and notify the sync
  // bridge if |source| is not ADDED_VIA_SYNC.
  void AddEntry(scoped_refptr<ReadingListEntry> entry,
                reading_list::EntrySource source);

  // API specifically for changes received via sync.
  ReadingListEntry* SyncMergeEntry(scoped_refptr<ReadingListEntry> entry);
  void SyncRemoveEntry(const GURL& url);
  void SyncDeleteAllEntriesAndSyncMetadata();

  class ScopedReadingListBatchUpdateImpl : public ScopedReadingListBatchUpdate,
                                           public ReadingListModelObserver {
   public:
    explicit ScopedReadingListBatchUpdateImpl(ReadingListModelImpl* model);
    ~ScopedReadingListBatchUpdateImpl() override;

    syncer::MetadataChangeList* GetSyncMetadataChangeList();
    ReadingListModelStorage::ScopedBatchUpdate* GetStorageBatch();

    // ReadingListModelObserver overrides.
    void ReadingListModelLoaded(const ReadingListModel* model) override;
    void ReadingListModelBeingShutdown(const ReadingListModel* model) override;

   private:
    raw_ptr<ReadingListModelImpl> model_;
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> storage_token_;
  };

  // If an entry exists with `url` and is unseen, it gets marked as seen (but
  // unread).
  void MarkEntrySeenIfExists(const GURL& url);

  // Same as BeginBatchUpdates(), but returns specifically
  // ReadingListModelImpl's ScopedReadingListBatchUpdateImpl.
  std::unique_ptr<ScopedReadingListBatchUpdateImpl>
  BeginBatchUpdatesWithSyncMetadata();

  // Returns true if the model is sync-ing with the server and the initial
  // download of data and corresponding merge has completed.
  bool IsTrackingSyncMetadata() const;

  static std::string TrimTitle(const std::string& title);

  // Test-only factory function to inject an arbitrary change processor.
  static std::unique_ptr<ReadingListModelImpl> BuildNewForTest(
      std::unique_ptr<ReadingListModelStorage> storage_layer,
      syncer::StorageType sync_storage_type_for_uma,
      syncer::WipeModelUponSyncDisabledBehavior
          wipe_model_upon_sync_disabled_behavior,
      base::Clock* clock,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  // Exposes the sync bridge publicly for testing purposes.
  ReadingListSyncBridge* GetSyncBridgeForTest();

 private:
  // An enum class to record storage state in enum histograms, or add it as a
  // suffix to metrics.
  enum class StorageStateForUma {
    // Account storage.
    kAccount = 0,
    // Local storage that is not being synced at the time the metric is
    // recorded.
    kLocalOnly = 1,
    // Local storage that is being synced at the time the metric is recorded.
    kSyncEnabled = 2,
    kMaxValue = kSyncEnabled
  };
  StorageStateForUma GetStorageStateForUma() const;
  std::string GetStorageStateSuffixForUma() const;

  ReadingListModelImpl(
      std::unique_ptr<ReadingListModelStorage> storage_layer,
      syncer::StorageType sync_storage_type_for_uma,
      syncer::WipeModelUponSyncDisabledBehavior
          wipe_model_upon_sync_disabled_behavior,
      base::Clock* clock,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  void StoreLoaded(ReadingListModelStorage::LoadResultOrError result_or_error);

  // Tells model that batch updates have completed. Called from
  // ScopedReadingListBatchUpdateImpl's destructor.
  void EndBatchUpdates();

  // Returns a mutable pointer to the entry with URL |url|. Return nullptr if
  // no entry is found.
  ReadingListEntry* GetMutableEntryFromURL(const GURL& url);

  // Returns the |storage_layer_| of the model.
  ReadingListModelStorage* StorageLayer();

  void MarkEntrySeenImpl(ReadingListEntry* entry);

  // Remove entry |url| and propagate to the sync bridge if |from_sync| is
  // false.
  void RemoveEntryByURLImpl(const GURL& url,
                            const base::Location& location,
                            bool from_sync);

  // Update the 3 counts above considering addition/removal of |entry|.
  void UpdateEntryStateCountersOnEntryRemoval(const ReadingListEntry& entry);
  void UpdateEntryStateCountersOnEntryInsertion(const ReadingListEntry& entry);

  void RecordCountMetrics(const std::string& event_suffix) const;

  const std::unique_ptr<ReadingListModelStorage> storage_layer_;
  const raw_ptr<base::Clock> clock_;

  ReadingListSyncBridge sync_bridge_;

  base::ObserverList<ReadingListModelObserver>::Unchecked observers_;

  bool loaded_ = false;

  // Used to suppress deletions and batch updates notifications when
  // ReadingListModelLoaded is not broadcasted yet.
  bool suppress_deletions_batch_updates_notifications_ = false;

  std::map<GURL, scoped_refptr<ReadingListEntry>> entries_;
  size_t unread_entry_count_ = 0;
  size_t read_entry_count_ = 0;
  size_t unseen_entry_count_ = 0;

  unsigned int current_batch_updates_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ReadingListModelImpl> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_IMPL_H_
