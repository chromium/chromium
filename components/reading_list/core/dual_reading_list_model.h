// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_DUAL_READING_LIST_MODEL_H_
#define COMPONENTS_READING_LIST_CORE_DUAL_READING_LIST_MODEL_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

namespace reading_list {

// ReadingListModel implementation that is capable of providing a merged view of
// two underlying instances of ReadingListModel. For newly-created entries, the
// class determines internally and based on sign-in & sync state, which
// instance should be used. It is useful to support sync-the-transport use-cases
// where the user is signed in but has sync turned off: in this case the two
// data sources (local entries and entries server-side) should be treated
// independently under the hood, but an in-memory merged view can be presented
// to UI layers and generally feature integrations.
class DualReadingListModel : public ReadingListModel,
                             public ReadingListModelObserver {
 public:
  enum class StorageStateForTesting {
    kNotFound,
    kExistsInAccountModelOnly,
    kExistsInLocalOrSyncableModelOnly,
    kExistsInBothModels
  };

  DualReadingListModel(
      std::unique_ptr<ReadingListModelImpl> local_or_syncable_model,
      std::unique_ptr<ReadingListModelImpl> account_model);
  ~DualReadingListModel() override;

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

  // ReadingListModelObserver overrides.
  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override;
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override;
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override;
  void ReadingListDidRemoveEntry(const ReadingListModel* model,
                                 const GURL& url) override;
  void ReadingListWillMoveEntry(const ReadingListModel* model,
                                const GURL& url) override;
  void ReadingListDidMoveEntry(const ReadingListModel* model,
                               const GURL& url) override;
  void ReadingListWillAddEntry(const ReadingListModel* model,
                               const ReadingListEntry& entry) override;
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource source) override;
  void ReadingListWillUpdateEntry(const ReadingListModel* model,
                                  const GURL& url) override;
  void ReadingListDidUpdateEntry(const ReadingListModel* model,
                                 const GURL& url) override;
  void ReadingListDidApplyChanges(ReadingListModel* model) override;

  class ScopedReadingListBatchUpdateImpl : public ScopedReadingListBatchUpdate {
   public:
    ScopedReadingListBatchUpdateImpl(
        std::unique_ptr<ScopedReadingListBatchUpdate>
            local_or_syncable_model_batch,
        std::unique_ptr<ScopedReadingListBatchUpdate> account_model_batch);
    ~ScopedReadingListBatchUpdateImpl() override;

   private:
    std::unique_ptr<ScopedReadingListBatchUpdate>
        local_or_syncable_model_batch_;
    std::unique_ptr<ScopedReadingListBatchUpdate> account_model_batch_;
  };

  // Returns the list of reading list entries which exists in the local-only
  // storage and need explicit upload to the sync server.
  // Note: This should only be called if `account_model_` is the one used for
  // sync.
  base::flat_set<GURL> GetKeysThatNeedUploadToSyncServer() const;

  StorageStateForTesting GetStorageStateForURLForTesting(const GURL& url);

  // Returns the model responsible for the local/syncable reading list.
  ReadingListModel* GetLocalOrSyncableModel();
  // Returns the model responsible for the account-bound reading list. This can
  // toggle between null and non-null at runtime depending on the sync/signin
  // state, and ReadingListModelCompletedBatchUpdates() will be called each time
  // it changes.
  ReadingListModel* GetAccountModelIfSyncing();

 private:
  void NotifyObserversWithWillRemoveEntry(const GURL& url);
  void NotifyObserversWithDidRemoveEntry(const GURL& url);
  void NotifyObserversWithWillMoveEntry(const GURL& url);
  void NotifyObserversWithDidMoveEntry(const GURL& url);
  void NotifyObserversWithWillUpdateEntry(const GURL& url);
  void NotifyObserversWithDidUpdateEntry(const GURL& url);
  void NotifyObserversWithDidApplyChanges();

  // Convenience function that safely "casts" to ReadingListModelImpl for
  // codepaths where model is guaranteed to be either local_or_syncable_model_
  // or account_model_.
  const ReadingListModelImpl* ToReadingListModelImpl(
      const ReadingListModel* model);

  // Update the unseen/unread/read entry counts considering addition/removal of
  // `entry` and updates applied to it.
  void UpdateEntryStateCountersOnEntryRemoval(const ReadingListEntry& entry);
  void UpdateEntryStateCountersOnEntryInsertion(const ReadingListEntry& entry);

  const std::unique_ptr<ReadingListModelImpl> local_or_syncable_model_;
  const std::unique_ptr<ReadingListModelImpl> account_model_;

  // Indicates whether the DualReadingListModel is currently handling the
  // notifications.
  bool suppress_observer_notifications_ = false;

  size_t unread_entry_count_ = 0;
  size_t read_entry_count_ = 0;
  size_t unseen_entry_count_ = 0;

  unsigned int current_batch_updates_count_ = 0;

  base::ObserverList<ReadingListModelObserver>::Unchecked observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_CORE_DUAL_READING_LIST_MODEL_H_
