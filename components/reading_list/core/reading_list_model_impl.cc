// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model_impl.h"

#include "base/bind.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "components/reading_list/core/reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_sync_bridge.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "url/gurl.h"

ReadingListModelImpl::ScopedReadingListBatchUpdateImpl::
    ScopedReadingListBatchUpdateImpl(ReadingListModelImpl* model)
    : model_(model) {
  model->AddObserver(this);
  if (model->StorageLayer()) {
    storage_token_ = model->StorageLayer()->EnsureBatchCreated();
    DCHECK(storage_token_);
  }
}

ReadingListModelImpl::ScopedReadingListBatchUpdateImpl::
    ~ScopedReadingListBatchUpdateImpl() {
  storage_token_.reset();
  if (model_) {
    model_->RemoveObserver(this);
    model_->EndBatchUpdates();
  }
}

syncer::MetadataChangeList* ReadingListModelImpl::
    ScopedReadingListBatchUpdateImpl::GetSyncMetadataChangeList() {
  DCHECK(storage_token_);
  return storage_token_->GetSyncMetadataChangeList();
}

void ReadingListModelImpl::ScopedReadingListBatchUpdateImpl::
    ReadingListModelLoaded(const ReadingListModel* model) {}

void ReadingListModelImpl::ScopedReadingListBatchUpdateImpl::
    ReadingListModelBeingShutdown(const ReadingListModel* model) {
  storage_token_.reset();
  model_->EndBatchUpdates();
  model_->RemoveObserver(this);
  model_ = nullptr;
}

ReadingListModelImpl::ReadingListModelImpl(
    std::unique_ptr<ReadingListModelStorage> storage_layer,
    PrefService* pref_service,
    base::Clock* clock)
    : ReadingListModelImpl(
          std::move(storage_layer),
          clock,
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::READING_LIST,
              /*dump_stack=*/base::DoNothing())) {}

ReadingListModelImpl::ReadingListModelImpl(
    std::unique_ptr<ReadingListModelStorage> storage_layer,
    base::Clock* clock,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : storage_layer_(std::move(storage_layer)),
      clock_(clock),
      sync_bridge_(clock, std::move(change_processor)) {
  DCHECK(clock_);
  if (storage_layer_) {
    storage_layer_->Load(clock_,
                         base::BindOnce(&ReadingListModelImpl::StoreLoaded,
                                        weak_ptr_factory_.GetWeakPtr()));
  } else {
    // TODO(crbug.com/1386158): Require a non-null storage instead of supporting
    // this test-only path. After all tests, can trivially adopt a fake.
    CHECK_IS_TEST();
    loaded_ = true;
    sync_bridge_.ModelReadyToSync(/*model=*/this,
                                  std::make_unique<syncer::MetadataBatch>());
  }
}

ReadingListModelImpl::~ReadingListModelImpl() {
  for (auto& observer : observers_) {
    observer.ReadingListModelBeingDeleted(this);
  }
}

void ReadingListModelImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.ReadingListModelBeingShutdown(this);
  loaded_ = false;
}

bool ReadingListModelImpl::loaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return loaded_;
}

bool ReadingListModelImpl::IsPerformingBatchUpdates() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_batch_updates_count_ > 0;
}

std::unique_ptr<ReadingListModelImpl::ScopedReadingListBatchUpdate>
ReadingListModelImpl::BeginBatchUpdates() {
  return BeginBatchUpdatesWithSyncMetadata();
}

size_t ReadingListModelImpl::size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_entry_count_ + unread_entry_count_ == entries_.size());
  if (!loaded())
    return 0;
  return entries_.size();
}

size_t ReadingListModelImpl::unread_size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_entry_count_ + unread_entry_count_ == entries_.size());
  if (!loaded())
    return 0;
  return unread_entry_count_;
}

size_t ReadingListModelImpl::unseen_size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loaded())
    return 0;
  return unseen_entry_count_;
}

void ReadingListModelImpl::MarkAllSeen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  if (unseen_entry_count_ == 0) {
    return;
  }
  std::unique_ptr<ReadingListModelImpl::ScopedReadingListBatchUpdate>
      model_batch_updates = BeginBatchUpdates();
  for (auto& iterator : entries_) {
    ReadingListEntry& entry = iterator.second;
    if (entry.HasBeenSeen()) {
      continue;
    }
    for (auto& observer : observers_) {
      observer.ReadingListWillUpdateEntry(this, iterator.first);
    }
    UpdateEntryStateCountersOnEntryRemoval(entry);
    entry.SetRead(false, clock_->Now());
    UpdateEntryStateCountersOnEntryInsertion(entry);
    if (storage_layer_) {
      // TODO(crbug.com/1386158): Reuse same batch.
      std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
          storage_layer_->EnsureBatchCreated();
      batch->SaveEntry(entry);
      sync_bridge_.DidAddOrUpdateEntry(entry,
                                       batch->GetSyncMetadataChangeList());
    }
    for (auto& observer : observers_) {
      observer.ReadingListDidApplyChanges(this);
    }
  }
  DCHECK(unseen_entry_count_ == 0);
}

bool ReadingListModelImpl::DeleteAllEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loaded()) {
    return false;
  }
  auto scoped_model_batch_updates = BeginBatchUpdates();
  for (const auto& url : Keys()) {
    RemoveEntryByURL(url);
  }
  return entries_.empty();
}

void ReadingListModelImpl::UpdateEntryStateCountersOnEntryRemoval(
    const ReadingListEntry& entry) {
  if (!entry.HasBeenSeen()) {
    unseen_entry_count_--;
  }
  if (entry.IsRead()) {
    read_entry_count_--;
  } else {
    unread_entry_count_--;
  }
}

void ReadingListModelImpl::UpdateEntryStateCountersOnEntryInsertion(
    const ReadingListEntry& entry) {
  if (!entry.HasBeenSeen()) {
    unseen_entry_count_++;
  }
  if (entry.IsRead()) {
    read_entry_count_++;
  } else {
    unread_entry_count_++;
  }
}

const std::vector<GURL> ReadingListModelImpl::Keys() const {
  std::vector<GURL> keys;
  for (const auto& iterator : entries_) {
    keys.push_back(iterator.first);
  }
  return keys;
}

const ReadingListEntry* ReadingListModelImpl::GetEntryByURL(
    const GURL& gurl) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  return const_cast<ReadingListModelImpl*>(this)->GetMutableEntryFromURL(gurl);
}

const ReadingListEntry* ReadingListModelImpl::GetFirstUnreadEntry(
    bool distilled) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  if (unread_entry_count_ == 0) {
    return nullptr;
  }
  int64_t update_time_all = 0;
  const ReadingListEntry* first_entry_all = nullptr;
  int64_t update_time_distilled = 0;
  const ReadingListEntry* first_entry_distilled = nullptr;
  for (auto& iterator : entries_) {
    const ReadingListEntry& entry = iterator.second;
    if (entry.IsRead()) {
      continue;
    }
    if (entry.UpdateTime() > update_time_all) {
      update_time_all = entry.UpdateTime();
      first_entry_all = &entry;
    }
    if (entry.DistilledState() == ReadingListEntry::PROCESSED &&
        entry.UpdateTime() > update_time_distilled) {
      update_time_distilled = entry.UpdateTime();
      first_entry_distilled = &entry;
    }
  }
  DCHECK(first_entry_all);
  DCHECK_GT(update_time_all, 0);
  if (distilled && first_entry_distilled) {
    return first_entry_distilled;
  }
  return first_entry_all;
}

ReadingListEntry* ReadingListModelImpl::GetMutableEntryFromURL(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return nullptr;
  }
  return &(iterator->second);
}

void ReadingListModelImpl::SyncAddEntry(
    std::unique_ptr<ReadingListEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  DCHECK(IsPerformingBatchUpdates());

  AddEntryImpl(std::move(entry), reading_list::ADDED_VIA_SYNC);
}

ReadingListEntry* ReadingListModelImpl::SyncMergeEntry(
    std::unique_ptr<ReadingListEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  DCHECK(IsPerformingBatchUpdates());

  const GURL url = entry->URL();
  ReadingListEntry* existing_entry = GetMutableEntryFromURL(url);
  DCHECK(existing_entry);

  for (auto& observer : observers_)
    observer.ReadingListWillMoveEntry(this, url);

  UpdateEntryStateCountersOnEntryRemoval(*existing_entry);
  existing_entry->MergeWithEntry(*entry);
  existing_entry = GetMutableEntryFromURL(url);
  UpdateEntryStateCountersOnEntryInsertion(*existing_entry);

  // Write to the store.
  if (storage_layer_) {
    storage_layer_->EnsureBatchCreated()->SaveEntry(*existing_entry);
  }

  for (auto& observer : observers_) {
    observer.ReadingListDidMoveEntry(this, url);
    observer.ReadingListDidApplyChanges(this);
  }

  return existing_entry;
}

void ReadingListModelImpl::SyncRemoveEntry(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  DCHECK(IsPerformingBatchUpdates());

  RemoveEntryByURLImpl(url, true);
}

void ReadingListModelImpl::RemoveEntryByURL(const GURL& url) {
  RemoveEntryByURLImpl(url, false);
}

void ReadingListModelImpl::RemoveEntryByURLImpl(const GURL& url,
                                                bool from_sync) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  const ReadingListEntry* entry = GetEntryByURL(url);
  if (!entry)
    return;

  for (auto& observer : observers_)
    observer.ReadingListWillRemoveEntry(this, url);

  if (storage_layer_) {
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
        storage_layer_->EnsureBatchCreated();
    batch->RemoveEntry(url);

    if (!from_sync) {
      sync_bridge_.DidRemoveEntry(*entry, batch->GetSyncMetadataChangeList());
    }
  }

  UpdateEntryStateCountersOnEntryRemoval(*entry);

  entries_.erase(url);
  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);
}

bool ReadingListModelImpl::IsUrlSupported(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

const ReadingListEntry& ReadingListModelImpl::AddEntry(
    const GURL& url,
    const std::string& title,
    reading_list::EntrySource source,
    base::TimeDelta estimated_read_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  DCHECK(IsUrlSupported(url));

  std::unique_ptr<ReadingListModelImpl::ScopedReadingListBatchUpdate>
      scoped_model_batch_updates;
  if (GetEntryByURL(url)) {
    scoped_model_batch_updates = BeginBatchUpdates();
    RemoveEntryByURL(url);
  }

  std::string trimmed_title = base::CollapseWhitespaceASCII(title, false);

  auto entry =
      std::make_unique<ReadingListEntry>(url, trimmed_title, clock_->Now());
  if (!estimated_read_time.is_zero()) {
    entry->SetEstimatedReadTime(estimated_read_time);
  }

  AddEntryImpl(std::move(entry), source);

  return entries_.at(url);
}

const ReadingListEntry& ReadingListModelImpl::AddEntry(
    const GURL& url,
    const std::string& title,
    reading_list::EntrySource source) {
  return AddEntry(url, title, source, base::TimeDelta());
}

void ReadingListModelImpl::SetReadStatus(const GURL& url, bool read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  if (entry.IsRead() == read) {
    return;
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillMoveEntry(this, url);
  }
  UpdateEntryStateCountersOnEntryRemoval(entry);
  entry.SetRead(read, clock_->Now());
  entry.MarkEntryUpdated(clock_->Now());
  UpdateEntryStateCountersOnEntryInsertion(entry);

  if (storage_layer_) {
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
        storage_layer_->EnsureBatchCreated();
    batch->SaveEntry(entry);
    sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidMoveEntry(this, url);
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEntryTitle(const GURL& url,
                                         const std::string& title) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  std::string trimmed_title = base::CollapseWhitespaceASCII(title, false);
  if (entry.Title() == trimmed_title) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetTitle(trimmed_title, clock_->Now());
  if (storage_layer_) {
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
        storage_layer_->EnsureBatchCreated();
    batch->SaveEntry(entry);
    sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEstimatedReadTime(
    const GURL& url,
    base::TimeDelta estimated_read_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  if (entry.EstimatedReadTime() == estimated_read_time) {
    return;
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetEstimatedReadTime(estimated_read_time);
  if (storage_layer_) {
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
        storage_layer_->EnsureBatchCreated();
    batch->SaveEntry(entry);
    sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEntryDistilledInfo(
    const GURL& url,
    const base::FilePath& distilled_path,
    const GURL& distilled_url,
    int64_t distillation_size,
    const base::Time& distillation_date) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  if (entry.DistilledState() == ReadingListEntry::PROCESSED &&
      entry.DistilledPath() == distilled_path) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetDistilledInfo(distilled_path, distilled_url, distillation_size,
                         distillation_date);
  if (storage_layer_) {
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
        storage_layer_->EnsureBatchCreated();
    batch->SaveEntry(entry);
    sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEntryDistilledState(
    const GURL& url,
    ReadingListEntry::DistillationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  if (entry.DistilledState() == state) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetDistilledState(state);
  if (storage_layer_) {
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
        storage_layer_->EnsureBatchCreated();
    batch->SaveEntry(entry);
    sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetContentSuggestionsExtra(
    const GURL& url,
    const reading_list::ContentSuggestionsExtra& extra) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  ReadingListEntry* entry = GetMutableEntryFromURL(url);
  if (!entry) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }

  entry->SetContentSuggestionsExtra(extra);
  if (storage_layer_) {
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
        storage_layer_->EnsureBatchCreated();
    batch->SaveEntry(*entry);
    sync_bridge_.DidAddOrUpdateEntry(*entry,
                                     batch->GetSyncMetadataChangeList());
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::AddObserver(ReadingListModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (loaded()) {
    observer->ReadingListModelLoaded(this);
  }
}

void ReadingListModelImpl::RemoveObserver(ReadingListModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ReadingListModelImpl::ScopedReadingListBatchUpdateImpl>
ReadingListModelImpl::BeginBatchUpdatesWithSyncMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto token = std::make_unique<ScopedReadingListBatchUpdateImpl>(this);
  ++current_batch_updates_count_;
  if (current_batch_updates_count_ == 1) {
    for (auto& observer : observers_) {
      observer.ReadingListModelBeganBatchUpdates(this);
    }
  }
  return token;
}

// static
std::unique_ptr<ReadingListModelImpl> ReadingListModelImpl::BuildNewForTest(
    std::unique_ptr<ReadingListModelStorage> storage_layer,
    PrefService* pref_service,
    base::Clock* clock,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor) {
  CHECK_IS_TEST();
  return base::WrapUnique(new ReadingListModelImpl(
      std::move(storage_layer), clock, std::move(change_processor)));
}

void ReadingListModelImpl::StoreLoaded(
    ReadingListModelStorage::LoadResultOrError result_or_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result_or_error.has_value()) {
    sync_bridge_.ReportError(
        syncer::ModelError(FROM_HERE, result_or_error.error()));
    return;
  }

  entries_ = std::move(result_or_error.value().first);

  for (auto& iterator : entries_) {
    UpdateEntryStateCountersOnEntryInsertion(iterator.second);
  }

  DCHECK_EQ(read_entry_count_ + unread_entry_count_, entries_.size());
  loaded_ = true;

  sync_bridge_.ModelReadyToSync(/*model=*/this,
                                std::move(result_or_error.value().second));

  base::UmaHistogramCounts1000("ReadingList.Unread.Count.OnModelLoaded",
                               unread_entry_count_);
  base::UmaHistogramCounts1000("ReadingList.Read.Count.OnModelLoaded",
                               read_entry_count_);

  for (auto& observer : observers_) {
    observer.ReadingListModelLoaded(this);
  }
}

void ReadingListModelImpl::EndBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsPerformingBatchUpdates());
  DCHECK(current_batch_updates_count_ > 0);
  --current_batch_updates_count_;
  if (current_batch_updates_count_ == 0) {
    for (auto& observer : observers_) {
      observer.ReadingListModelCompletedBatchUpdates(this);
    }
  }
}

ReadingListSyncBridge* ReadingListModelImpl::GetModelTypeSyncBridge() {
  return &sync_bridge_;
}

ReadingListModelStorage* ReadingListModelImpl::StorageLayer() {
  return storage_layer_.get();
}

void ReadingListModelImpl::AddEntryImpl(std::unique_ptr<ReadingListEntry> entry,
                                        reading_list::EntrySource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entry);
  DCHECK(loaded());
  DCHECK(GetMutableEntryFromURL(entry->URL()) == nullptr);

  const GURL url = entry->URL();

  for (auto& observer : observers_) {
    observer.ReadingListWillAddEntry(this, *entry);
  }

  UpdateEntryStateCountersOnEntryInsertion(*entry);

  auto it = entries_.emplace(url, std::move(*entry)).first;
  const ReadingListEntry* entry_ptr = &it->second;

  if (storage_layer_) {
    std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
        storage_layer_->EnsureBatchCreated();
    batch->SaveEntry(*GetEntryByURL(url));
    if (source != reading_list::ADDED_VIA_SYNC) {
      sync_bridge_.DidAddOrUpdateEntry(*entry_ptr,
                                       batch->GetSyncMetadataChangeList());
    }
  }

  for (auto& observer : observers_) {
    observer.ReadingListDidAddEntry(this, url, source);
    observer.ReadingListDidApplyChanges(this);
  }
}
