// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model_impl.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "components/reading_list/core/reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_sync_bridge.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "google_apis/gaia/core_account_id.h"
#include "url/gurl.h"

ReadingListModelImpl::ScopedReadingListBatchUpdateImpl::
    ScopedReadingListBatchUpdateImpl(ReadingListModelImpl* model)
    : model_(model) {
  model->AddObserver(this);
  storage_token_ = model->StorageLayer()->EnsureBatchCreated();
  DCHECK(storage_token_);
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

ReadingListModelStorage::ScopedBatchUpdate*
ReadingListModelImpl::ScopedReadingListBatchUpdateImpl::GetStorageBatch() {
  DCHECK(storage_token_);
  return storage_token_.get();
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
    syncer::StorageType sync_storage_type_for_uma,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior,
    base::Clock* clock)
    : ReadingListModelImpl(
          std::move(storage_layer),
          sync_storage_type_for_uma,
          wipe_model_upon_sync_disabled_behavior,
          clock,
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::READING_LIST,
              /*dump_stack=*/base::DoNothing())) {}

ReadingListModelImpl::ReadingListModelImpl(
    std::unique_ptr<ReadingListModelStorage> storage_layer,
    syncer::StorageType sync_storage_type_for_uma,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior,
    base::Clock* clock,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : storage_layer_(std::move(storage_layer)),
      clock_(clock),
      sync_bridge_(sync_storage_type_for_uma,
                   wipe_model_upon_sync_disabled_behavior,
                   clock,
                   std::move(change_processor)) {
  DCHECK(clock_);
  DCHECK(storage_layer_);

  storage_layer_->Load(clock_,
                       base::BindOnce(&ReadingListModelImpl::StoreLoaded,
                                      weak_ptr_factory_.GetWeakPtr()));
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

  std::unique_ptr<ScopedReadingListBatchUpdateImpl> batch =
      BeginBatchUpdatesWithSyncMetadata();

  for (auto& iterator : entries_) {
    MarkEntrySeenImpl(iterator.second.get());
  }
  DCHECK(unseen_entry_count_ == 0);
}

bool ReadingListModelImpl::DeleteAllEntries(const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loaded()) {
    return false;
  }
  auto scoped_model_batch_updates = BeginBatchUpdates();
  for (const auto& url : GetKeys()) {
    RemoveEntryByURL(url, location);
  }

  DCHECK(entries_.empty());
  return true;
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

base::flat_set<GURL> ReadingListModelImpl::GetKeys() const {
  std::vector<GURL> keys;
  keys.reserve(entries_.size());
  for (const auto& url_and_entry : entries_) {
    keys.push_back(url_and_entry.first);
  }
  return base::flat_set<GURL>(base::sorted_unique, std::move(keys));
}

scoped_refptr<const ReadingListEntry> ReadingListModelImpl::GetEntryByURL(
    const GURL& gurl) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  return base::WrapRefCounted(
      const_cast<ReadingListModelImpl*>(this)->GetMutableEntryFromURL(gurl));
}

ReadingListEntry* ReadingListModelImpl::GetMutableEntryFromURL(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return nullptr;
  }
  return iterator->second.get();
}

ReadingListEntry* ReadingListModelImpl::SyncMergeEntry(
    scoped_refptr<ReadingListEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  DCHECK(IsPerformingBatchUpdates());

  const GURL url = entry->URL();
  ReadingListEntry* existing_entry = GetMutableEntryFromURL(url);
  DCHECK(existing_entry);

  // TODO(crbug.com/40260548): ReadingList(Will|Did)MoveEntry() in this context
  // is quite meaningless and the observer API should merge it with
  // ReadingList(Will|Did)UpdateEntry().

  for (auto& observer : observers_)
    observer.ReadingListWillMoveEntry(this, url);

  UpdateEntryStateCountersOnEntryRemoval(*existing_entry);
  existing_entry->MergeWithEntry(*entry);
  existing_entry = GetMutableEntryFromURL(url);
  UpdateEntryStateCountersOnEntryInsertion(*existing_entry);

  // Write to the store.
  storage_layer_->EnsureBatchCreated()->SaveEntry(*existing_entry);

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

  RemoveEntryByURLImpl(url, FROM_HERE, true);
}

void ReadingListModelImpl::RemoveEntryByURL(const GURL& url,
                                            const base::Location& location) {
  RemoveEntryByURLImpl(url, location, false);
}

void ReadingListModelImpl::RemoveEntryByURLImpl(const GURL& url,
                                                const base::Location& location,
                                                bool from_sync) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
  if (!entry)
    return;

  if (!suppress_deletions_batch_updates_notifications_) {
    for (auto& observer : observers_) {
      observer.ReadingListWillRemoveEntry(this, url);
    }
  }

  std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
      storage_layer_->EnsureBatchCreated();
  batch->RemoveEntry(url);

  if (!from_sync) {
    sync_bridge_.DidRemoveEntry(*entry, location,
                                batch->GetSyncMetadataChangeList());
  }

  UpdateEntryStateCountersOnEntryRemoval(*entry);

  entries_.erase(url);

  if (!suppress_deletions_batch_updates_notifications_) {
    for (auto& observer : observers_) {
      observer.ReadingListDidRemoveEntry(this, url);
      observer.ReadingListDidApplyChanges(this);
    }
  }
}

void ReadingListModelImpl::SyncDeleteAllEntriesAndSyncMetadata() {
  DeleteAllEntries(FROM_HERE);
  storage_layer_->DeleteAllEntriesAndSyncMetadata();
}

bool ReadingListModelImpl::IsUrlSupported(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

CoreAccountId ReadingListModelImpl::GetAccountWhereEntryIsSavedTo(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  if (entries_.find(url) == entries_.end()) {
    return CoreAccountId();
  }
  return CoreAccountId::FromString(
      sync_bridge_.change_processor()->TrackedAccountId());
}

bool ReadingListModelImpl::NeedsExplicitUploadToSyncServer(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Returning true only makes sense for an implementation that maintains a
  // separate set of local and account entries (DualReadingListModel).
  return false;
}

void ReadingListModelImpl::MarkAllForUploadToSyncServerIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Uploading the entries only makes sense for an implementation that maintains
  // a separate set of local and account entries (DualReadingListModel).
}

const ReadingListEntry& ReadingListModelImpl::AddOrReplaceEntry(
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
    RemoveEntryByURL(url, FROM_HERE);
  }

  std::string trimmed_title = TrimTitle(title);

  auto entry =
      base::MakeRefCounted<ReadingListEntry>(url, trimmed_title, clock_->Now());
  if (!estimated_read_time.is_zero()) {
    entry->SetEstimatedReadTime(estimated_read_time);
  }

  AddEntry(std::move(entry), source);

  base::UmaHistogramEnumeration("ReadingList.AddOrReplaceEntry",
                                GetStorageStateForUma());

  return *(entries_.at(url));
}

void ReadingListModelImpl::SetReadStatusIfExists(const GURL& url, bool read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = *(iterator->second);
  if (entry.IsRead() == read) {
    return;
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillMoveEntry(this, url);
  }
  UpdateEntryStateCountersOnEntryRemoval(entry);
  entry.SetRead(read, clock_->Now());
  UpdateEntryStateCountersOnEntryInsertion(entry);

  std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
      storage_layer_->EnsureBatchCreated();
  batch->SaveEntry(entry);
  sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidMoveEntry(this, url);
    observer.ReadingListDidApplyChanges(this);
  }

  if (read) {
    base::UmaHistogramEnumeration("ReadingList.MarkEntryRead",
                                  GetStorageStateForUma());
  }
}

void ReadingListModelImpl::SetEntryTitleIfExists(const GURL& url,
                                                 const std::string& title) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = *(iterator->second);
  std::string trimmed_title = TrimTitle(title);
  if (entry.Title() == trimmed_title) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetTitle(trimmed_title, clock_->Now());

  std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
      storage_layer_->EnsureBatchCreated();
  batch->SaveEntry(entry);
  sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidUpdateEntry(this, url);
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEstimatedReadTimeIfExists(
    const GURL& url,
    base::TimeDelta estimated_read_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = *(iterator->second);
  if (entry.EstimatedReadTime() == estimated_read_time) {
    return;
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetEstimatedReadTime(estimated_read_time);

  std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
      storage_layer_->EnsureBatchCreated();
  batch->SaveEntry(entry);
  sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidUpdateEntry(this, url);
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEntryDistilledInfoIfExists(
    const GURL& url,
    const base::FilePath& distilled_path,
    const GURL& distilled_url,
    int64_t distillation_size,
    base::Time distillation_date) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = *(iterator->second);
  if (entry.DistilledState() == ReadingListEntry::PROCESSED &&
      entry.DistilledPath() == distilled_path) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetDistilledInfo(distilled_path, distilled_url, distillation_size,
                         distillation_date);

  std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
      storage_layer_->EnsureBatchCreated();
  batch->SaveEntry(entry);
  sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidUpdateEntry(this, url);
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEntryDistilledStateIfExists(
    const GURL& url,
    ReadingListEntry::DistillationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  ReadingListEntry& entry = *(iterator->second);
  if (entry.DistilledState() == state) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetDistilledState(state);

  std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
      storage_layer_->EnsureBatchCreated();
  batch->SaveEntry(entry);
  sync_bridge_.DidAddOrUpdateEntry(entry, batch->GetSyncMetadataChangeList());

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidUpdateEntry(this, url);
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

void ReadingListModelImpl::RecordCountMetricsOnUMAUpload() const {
  if (!loaded()) {
    return;
  }
  RecordCountMetrics(".OnUMAUpload");
}

void ReadingListModelImpl::AddEntry(scoped_refptr<ReadingListEntry> entry,
                                    reading_list::EntrySource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entry);
  DCHECK(loaded());
  DCHECK(GetMutableEntryFromURL(entry->URL()) == nullptr);

  // TODO(crbug.com/40899983): Should decide if the DCHECK(entry) should be
  // removed or there's a proper fix that remove the below condition.
  if (!entry) {
    return;
  }

  const GURL url = entry->URL();

  for (auto& observer : observers_) {
    observer.ReadingListWillAddEntry(this, *entry);
  }

  UpdateEntryStateCountersOnEntryInsertion(*entry);

  auto it = entries_.emplace(url, std::move(entry)).first;
  const ReadingListEntry* entry_ptr = it->second.get();

  std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
      storage_layer_->EnsureBatchCreated();
  batch->SaveEntry(*GetEntryByURL(url));
  if (source != reading_list::ADDED_VIA_SYNC) {
    sync_bridge_.DidAddOrUpdateEntry(*entry_ptr,
                                     batch->GetSyncMetadataChangeList());
  }

  for (auto& observer : observers_) {
    observer.ReadingListDidAddEntry(this, url, source);
    observer.ReadingListDidApplyChanges(this);
  }
}

std::unique_ptr<ReadingListModelImpl::ScopedReadingListBatchUpdateImpl>
ReadingListModelImpl::BeginBatchUpdatesWithSyncMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto token = std::make_unique<ScopedReadingListBatchUpdateImpl>(this);
  ++current_batch_updates_count_;
  if (current_batch_updates_count_ == 1 &&
      !suppress_deletions_batch_updates_notifications_) {
    for (auto& observer : observers_) {
      observer.ReadingListModelBeganBatchUpdates(this);
    }
  }
  return token;
}

void ReadingListModelImpl::MarkEntrySeenIfExists(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  auto iterator = entries_.find(url);
  if (iterator == entries_.end()) {
    return;
  }
  MarkEntrySeenImpl(iterator->second.get());
}

bool ReadingListModelImpl::IsTrackingSyncMetadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_bridge_.IsTrackingMetadata();
}

// static
std::string ReadingListModelImpl::TrimTitle(const std::string& title) {
  return base::CollapseWhitespaceASCII(title, false);
}

// static
std::unique_ptr<ReadingListModelImpl> ReadingListModelImpl::BuildNewForTest(
    std::unique_ptr<ReadingListModelStorage> storage_layer,
    syncer::StorageType sync_storage_type,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior,
    base::Clock* clock,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor) {
  CHECK_IS_TEST();
  return base::WrapUnique(
      new ReadingListModelImpl(std::move(storage_layer), sync_storage_type,
                               wipe_model_upon_sync_disabled_behavior, clock,
                               std::move(change_processor)));
}

ReadingListSyncBridge* ReadingListModelImpl::GetSyncBridgeForTest() {
  return &sync_bridge_;
}

ReadingListModelImpl::StorageStateForUma
ReadingListModelImpl::GetStorageStateForUma() const {
  switch (sync_bridge_.GetStorageTypeForUma()) {
    case syncer::StorageType::kAccount:
      return StorageStateForUma::kAccount;
    case syncer::StorageType::kUnspecified:
      return sync_bridge_.IsTrackingMetadata()
                 ? StorageStateForUma::kSyncEnabled
                 : StorageStateForUma::kLocalOnly;
  }
  NOTREACHED();
}

std::string ReadingListModelImpl::GetStorageStateSuffixForUma() const {
  switch (GetStorageStateForUma()) {
    case StorageStateForUma::kAccount:
      return ".AccountStorage";
    case StorageStateForUma::kLocalOnly:
      return ".LocalStorage";
    case StorageStateForUma::kSyncEnabled:
      return ".LocalStorageSyncing";
  }
  NOTREACHED();
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
    UpdateEntryStateCountersOnEntryInsertion(*(iterator.second));
  }

  DCHECK_EQ(read_entry_count_ + unread_entry_count_, entries_.size());
  loaded_ = true;

  RecordCountMetrics(".OnModelLoaded");

  {
    // In rare cases, ModelReadyToSync() leads to the deletion of all local
    // entries. Such deletions should not be propagated to observers, because
    // ReadingListModelLoaded hasn't been broadcasted yet.
    base::AutoReset<bool> auto_reset_suppress_observer_notifications(
        &suppress_deletions_batch_updates_notifications_, true);
    sync_bridge_.ModelReadyToSync(/*model=*/this,
                                  std::move(result_or_error.value().second));
  }

  for (auto& observer : observers_) {
    observer.ReadingListModelLoaded(this);
  }
}

void ReadingListModelImpl::EndBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsPerformingBatchUpdates());
  DCHECK(current_batch_updates_count_ > 0);
  --current_batch_updates_count_;
  if (current_batch_updates_count_ == 0 &&
      !suppress_deletions_batch_updates_notifications_) {
    for (auto& observer : observers_) {
      observer.ReadingListModelCompletedBatchUpdates(this);
    }
  }
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
ReadingListModelImpl::GetSyncControllerDelegate() {
  return sync_bridge_.change_processor()->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
ReadingListModelImpl::GetSyncControllerDelegateForTransportMode() {
  // ReadingListModelImpl doesn't directly implement account storage. Upper
  // layers are responsible for maintaining two instances of
  // ReadingListModelImpl and exposing one of them as account storage.
  return nullptr;
}

ReadingListModelStorage* ReadingListModelImpl::StorageLayer() {
  return storage_layer_.get();
}

void ReadingListModelImpl::MarkEntrySeenImpl(ReadingListEntry* entry) {
  DCHECK(entry);

  if (entry->HasBeenSeen()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, entry->URL());
  }

  UpdateEntryStateCountersOnEntryRemoval(*entry);
  DCHECK(!entry->IsRead());
  // SetRead() is used to transition the entry from the UNSEEN state to the
  // UNREAD state.
  entry->SetRead(false, clock_->Now());
  UpdateEntryStateCountersOnEntryInsertion(*entry);

  std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate> batch =
      storage_layer_->EnsureBatchCreated();
  batch->SaveEntry(*entry);
  sync_bridge_.DidAddOrUpdateEntry(*entry, batch->GetSyncMetadataChangeList());

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidUpdateEntry(this, entry->URL());
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::RecordCountMetrics(
    const std::string& event_suffix) const {
  CHECK(loaded());
  std::string storage_suffix = GetStorageStateSuffixForUma();
  base::UmaHistogramCounts1000(
      base::StrCat({"ReadingList.Unread.Count", event_suffix, storage_suffix}),
      unread_entry_count_);
  base::UmaHistogramCounts1000(
      base::StrCat({"ReadingList.Read.Count", event_suffix, storage_suffix}),
      read_entry_count_);
}
