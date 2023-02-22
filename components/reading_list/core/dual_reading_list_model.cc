// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/dual_reading_list_model.h"

#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "url/gurl.h"

namespace reading_list {

DualReadingListModel::ScopedReadingListBatchUpdateImpl::
    ScopedReadingListBatchUpdateImpl(
        std::unique_ptr<ScopedReadingListBatchUpdate>
            local_or_syncable_model_batch,
        std::unique_ptr<ScopedReadingListBatchUpdate> account_model_batch)
    : local_or_syncable_model_batch_(std::move(local_or_syncable_model_batch)),
      account_model_batch_(std::move(account_model_batch)) {}

DualReadingListModel::ScopedReadingListBatchUpdateImpl::
    ~ScopedReadingListBatchUpdateImpl() = default;

DualReadingListModel::DualReadingListModel(
    std::unique_ptr<ReadingListModelImpl> local_or_syncable_model,
    std::unique_ptr<ReadingListModelImpl> account_model)
    : local_or_syncable_model_(std::move(local_or_syncable_model)),
      account_model_(std::move(account_model)) {
  DCHECK(local_or_syncable_model_);
  DCHECK(account_model_);
  local_or_syncable_model_->AddObserver(this);
  account_model_->AddObserver(this);
}

DualReadingListModel::~DualReadingListModel() {
  local_or_syncable_model_->RemoveObserver(this);
  account_model_->RemoveObserver(this);
}

void DualReadingListModel::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_or_syncable_model_->Shutdown();
  account_model_->Shutdown();
}

bool DualReadingListModel::loaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_or_syncable_model_->loaded() && account_model_->loaded();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
DualReadingListModel::GetSyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_or_syncable_model_->GetSyncControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
DualReadingListModel::GetSyncControllerDelegateForTransportMode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402200): This logic should be moved to a controller and
  // made more sophisticated by enabling it only if the user opted in (possibly
  // pref-based).
  if (base::FeatureList::IsEnabled(
          switches::kReadingListEnableSyncTransportModeUponSignIn)) {
    return account_model_->GetSyncControllerDelegate();
  }

  // Otherwise, disable the datatype.
  return nullptr;
}

bool DualReadingListModel::IsPerformingBatchUpdates() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_batch_updates_count_ > 0;
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
DualReadingListModel::BeginBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<ScopedReadingListBatchUpdateImpl>(
      local_or_syncable_model_->BeginBatchUpdates(),
      account_model_->BeginBatchUpdates());
}

base::flat_set<GURL> DualReadingListModel::GetKeys() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::STLSetUnion<base::flat_set<GURL>>(
      local_or_syncable_model_->GetKeys(), account_model_->GetKeys());
}

size_t DualReadingListModel::size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // While an efficient universal solution isn't implemented, at least optimize
  // for the trivial (and most common) cases, which is the case where at least
  // one of the underlying instances is empty.
  if (local_or_syncable_model_->size() == 0) {
    return account_model_->size();
  }
  if (account_model_->size() == 0) {
    return local_or_syncable_model_->size();
  }
  // TODO(crbug.com/1402196): Implement more efficiently.
  return GetKeys().size();
}

size_t DualReadingListModel::unread_size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
  return 0UL;
}

size_t DualReadingListModel::unseen_size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
  return 0UL;
}

void DualReadingListModel::MarkAllSeen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
}

bool DualReadingListModel::DeleteAllEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
  return false;
}

scoped_refptr<const ReadingListEntry> DualReadingListModel::GetEntryByURL(
    const GURL& gurl) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<const ReadingListEntry> local_or_syncable_entry =
      local_or_syncable_model_->GetEntryByURL(gurl);
  scoped_refptr<const ReadingListEntry> account_entry =
      account_model_->GetEntryByURL(gurl);
  if (!local_or_syncable_entry) {
    return account_entry;
  }
  if (!account_entry) {
    return local_or_syncable_entry;
  }
  scoped_refptr<ReadingListEntry> merged_entry =
      local_or_syncable_entry->Clone();
  // Merging the account entry into the local one should result in the merged
  // view's distilled state being equal to the local entry's. This is because
  // the local entry must be older than the account entry, as local entries can
  // only be created while the user is signed out.
  merged_entry->MergeWithEntry(*account_entry);
  return merged_entry;
}

bool DualReadingListModel::IsUrlSupported(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(local_or_syncable_model_->IsUrlSupported(url),
            account_model_->IsUrlSupported(url));
  return local_or_syncable_model_->IsUrlSupported(url);
}

bool DualReadingListModel::NeedsExplicitUploadToSyncServer(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!local_or_syncable_model_->IsTrackingSyncMetadata() ||
         !account_model_->IsTrackingSyncMetadata());

  return account_model_->IsTrackingSyncMetadata() &&
         local_or_syncable_model_->GetEntryByURL(url) != nullptr &&
         account_model_->GetEntryByURL(url) == nullptr;
}

const ReadingListEntry& DualReadingListModel::AddOrReplaceEntry(
    const GURL& url,
    const std::string& title,
    reading_list::EntrySource source,
    base::TimeDelta estimated_read_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  DCHECK(IsUrlSupported(url));

  std::unique_ptr<DualReadingListModel::ScopedReadingListBatchUpdate>
      scoped_model_batch_updates;
  if (GetEntryByURL(url)) {
    scoped_model_batch_updates = BeginBatchUpdates();
    RemoveEntryByURL(url);
  }

  if (account_model_->IsTrackingSyncMetadata()) {
    return account_model_->AddOrReplaceEntry(url, title, source,
                                             estimated_read_time);
  }
  return local_or_syncable_model_->AddOrReplaceEntry(url, title, source,
                                                     estimated_read_time);
}

void DualReadingListModel::RemoveEntryByURL(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
  // If there is no entry with the given URL, then an early return is needed to
  // avoid notifying observers.
  if (!entry) {
    return;
  }

  NotifyObserversWithWillRemoveEntry(url);

  {
    base::AutoReset<bool> auto_reset_ongoing_remove_entry_by_url(
        &ongoing_remove_entry_by_url_, true);
    local_or_syncable_model_->RemoveEntryByURL(url);
    account_model_->RemoveEntryByURL(url);
  }

  NotifyObserversWithDidRemoveEntry(url);
  NotifyObserversWithDidApplyChanges();
}

void DualReadingListModel::SetReadStatusIfExists(const GURL& url, bool read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
}

void DualReadingListModel::SetEntryTitleIfExists(const GURL& url,
                                                 const std::string& title) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
}

void DualReadingListModel::SetEstimatedReadTimeIfExists(
    const GURL& url,
    base::TimeDelta estimated_read_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
}

void DualReadingListModel::SetEntryDistilledStateIfExists(
    const GURL& url,
    ReadingListEntry::DistillationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
}

void DualReadingListModel::SetEntryDistilledInfoIfExists(
    const GURL& url,
    const base::FilePath& distilled_path,
    const GURL& distilled_url,
    int64_t distilation_size,
    base::Time distilation_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
}

void DualReadingListModel::AddObserver(ReadingListModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (loaded()) {
    observer->ReadingListModelLoaded(this);
  }
}

void DualReadingListModel::RemoveObserver(ReadingListModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void DualReadingListModel::ReadingListModelBeganBatchUpdates(
    const ReadingListModel* model) {
  ++current_batch_updates_count_;
  if (current_batch_updates_count_ == 1) {
    for (auto& observer : observers_) {
      observer.ReadingListModelBeganBatchUpdates(this);
    }
  }
}

void DualReadingListModel::ReadingListModelCompletedBatchUpdates(
    const ReadingListModel* model) {
  --current_batch_updates_count_;
  if (current_batch_updates_count_ == 0) {
    for (auto& observer : observers_) {
      observer.ReadingListModelCompletedBatchUpdates(this);
    }
  }
}

void DualReadingListModel::ReadingListModelLoaded(
    const ReadingListModel* model) {
  if (loaded()) {
    for (auto& observer : observers_) {
      observer.ReadingListModelLoaded(this);
    }
  }
}

void DualReadingListModel::ReadingListWillRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if (!ongoing_remove_entry_by_url_) {
    NotifyObserversWithWillRemoveEntry(url);
  }
}

void DualReadingListModel::ReadingListDidRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if (!ongoing_remove_entry_by_url_) {
    NotifyObserversWithDidRemoveEntry(url);
  }
}

void DualReadingListModel::ReadingListWillAddEntry(
    const ReadingListModel* model,
    const ReadingListEntry& entry) {
  for (auto& observer : observers_) {
    observer.ReadingListWillAddEntry(this, entry);
  }
}

void DualReadingListModel::ReadingListDidAddEntry(
    const ReadingListModel* model,
    const GURL& url,
    reading_list::EntrySource source) {
  for (auto& observer : observers_) {
    observer.ReadingListDidAddEntry(this, url, source);
  }
}

void DualReadingListModel::ReadingListDidApplyChanges(ReadingListModel* model) {
  if (!ongoing_remove_entry_by_url_) {
    NotifyObserversWithDidApplyChanges();
  }
}

DualReadingListModel::StorageStateForTesting
DualReadingListModel::GetStorageStateForURLForTesting(const GURL& url) {
  const bool exists_in_local_or_syncable_model =
      local_or_syncable_model_->GetEntryByURL(url) != nullptr;
  const bool exists_in_account_model =
      account_model_->GetEntryByURL(url) != nullptr;
  if (exists_in_local_or_syncable_model && exists_in_account_model) {
    return StorageStateForTesting::kExistsInBothModels;
  }
  if (exists_in_local_or_syncable_model) {
    return StorageStateForTesting::kExistsInLocalOrSyncableModelOnly;
  }
  if (exists_in_account_model) {
    return StorageStateForTesting::kExistsInAccountModelOnly;
  }
  return StorageStateForTesting::kNotFound;
}

void DualReadingListModel::NotifyObserversWithWillRemoveEntry(const GURL& url) {
  for (auto& observer : observers_) {
    observer.ReadingListWillRemoveEntry(this, url);
  }
}

void DualReadingListModel::NotifyObserversWithDidRemoveEntry(const GURL& url) {
  for (auto& observer : observers_) {
    observer.ReadingListDidRemoveEntry(this, url);
  }
}

void DualReadingListModel::NotifyObserversWithDidApplyChanges() {
  for (auto& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

}  // namespace reading_list
