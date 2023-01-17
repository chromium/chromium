// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_DUAL_READING_LIST_MODEL_H_
#define COMPONENTS_READING_LIST_CORE_DUAL_READING_LIST_MODEL_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"

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
  DualReadingListModel(
      std::unique_ptr<ReadingListModel> local_or_syncable_model,
      std::unique_ptr<ReadingListModel> account_model);
  ~DualReadingListModel() override;

  // KeyedService implementation.
  void Shutdown() override;

  // ReadingListModel implementation.
  bool loaded() const override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetSyncControllerDelegate()
      override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegateForTransportMode() override;
  bool IsPerformingBatchUpdates() const override;
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

  // ReadingListModelObserver overrides.
  void ReadingListModelLoaded(const ReadingListModel* model) override;

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

 private:
  const std::unique_ptr<ReadingListModel> local_or_syncable_model_;
  const std::unique_ptr<ReadingListModel> account_model_;

  base::ObserverList<ReadingListModelObserver>::Unchecked observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_CORE_DUAL_READING_LIST_MODEL_H_
