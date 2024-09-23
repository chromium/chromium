// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/dual_reading_list_model.h"

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/features.h"
#include "google_apis/gaia/core_account_id.h"
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

base::WeakPtr<syncer::DataTypeControllerDelegate>
DualReadingListModel::GetSyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return local_or_syncable_model_->GetSyncControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
DualReadingListModel::GetSyncControllerDelegateForTransportMode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40251098): This logic should be moved to a controller and
  // made more sophisticated by enabling it only if the user opted in (possibly
  // pref-based).
  if (syncer::IsReadingListAccountStorageEnabled()) {
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
  DCHECK(loaded());
  DCHECK_EQ(unread_entry_count_ + read_entry_count_, GetKeys().size());

  return unread_entry_count_ + read_entry_count_;
}

size_t DualReadingListModel::unread_size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  return unread_entry_count_;
}

size_t DualReadingListModel::unseen_size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  return unseen_entry_count_;
}

void DualReadingListModel::MarkAllSeen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  std::unique_ptr<DualReadingListModel::ScopedReadingListBatchUpdate>
      scoped_model_batch_updates;
  if (unseen_entry_count_ != 0) {
    scoped_model_batch_updates = BeginBatchUpdates();
  }

  for (const auto& url : GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
    const bool notify_observers = !entry->HasBeenSeen();
    if (notify_observers) {
      NotifyObserversWithWillUpdateEntry(url);
      UpdateEntryStateCountersOnEntryRemoval(*entry);
    }

    {
      base::AutoReset<bool> auto_reset_suppress_observer_notifications(
          &suppress_observer_notifications_, true);
      local_or_syncable_model_->MarkEntrySeenIfExists(url);
      account_model_->MarkEntrySeenIfExists(url);
    }

    if (notify_observers) {
      UpdateEntryStateCountersOnEntryInsertion(*GetEntryByURL(url));
      NotifyObserversWithDidUpdateEntry(url);
      NotifyObserversWithDidApplyChanges();
    }
  }

  DCHECK_EQ(unseen_entry_count_, 0ul);
}

bool DualReadingListModel::DeleteAllEntries(const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded()) {
    return false;
  }
  // Invoking DeleteAllEntries() for the two underlying instances would be the
  // most straightforward implementation, but it will complicate the observer
  // notifications. The simplest implementation would be leaning on
  // DualReadingListModel::RemoveEntryByURL to remove the entries.
  std::unique_ptr<DualReadingListModel::ScopedReadingListBatchUpdate>
      scoped_model_batch_updates = BeginBatchUpdates();
  for (const auto& url : GetKeys()) {
    RemoveEntryByURL(url, location);
  }

  DCHECK_EQ(0u, local_or_syncable_model_->size());
  DCHECK_EQ(0u, account_model_->size());
  return true;
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

CoreAccountId DualReadingListModel::GetAccountWhereEntryIsSavedTo(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  CoreAccountId account_id = account_model_->GetAccountWhereEntryIsSavedTo(url);
  if (!account_id.empty()) {
    return account_id;
  }
  // `local_or_syncable_model_` may return an account for the case where it's
  // sync-ing.
  return local_or_syncable_model_->GetAccountWhereEntryIsSavedTo(url);
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

void DualReadingListModel::MarkAllForUploadToSyncServerIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!account_model_->IsTrackingSyncMetadata()) {
    return;
  }

  base::AutoReset<bool> auto_reset_suppress_observer_notifications(
      &suppress_observer_notifications_, true);

  for (const GURL& url : local_or_syncable_model_->GetKeys()) {
    scoped_refptr<ReadingListEntry> entry = GetEntryByURL(url)->Clone();
    local_or_syncable_model_->RemoveEntryByURL(url, FROM_HERE);
    // If the url already exists in the account model, remove the account entry
    // first before adding the "merged" entry back to the account model.
    // Note: This workaround is used than just using AddOrReplaceEntry() to
    // avoid ReadingListModelBeganBatchUpdates() being triggered inside
    // AddOrReplaceEntry(), which causes observers to be notified even though
    // this particular function does not need to send any notifications at all
    // (including ReadingListModelBeganBatchUpdates).
    account_model_->RemoveEntryByURL(url, FROM_HERE);
    account_model_->AddEntry(std::move(entry),
                             reading_list::ADDED_VIA_CURRENT_APP);
    // The entry state counters do not need to updated since no value was
    // "effectively" removed from the dual reading list model.
  }
  // Ensure that the local model is empty since all the entries should have been
  // moved to the account model, including the common entries.
  CHECK_EQ(0u, local_or_syncable_model_->size());
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
    RemoveEntryByURL(url, FROM_HERE);
  }

  if (account_model_->IsTrackingSyncMetadata()) {
    const ReadingListEntry& entry = account_model_->AddOrReplaceEntry(
        url, title, source, estimated_read_time);
    DCHECK(!GetAccountWhereEntryIsSavedTo(url).empty());
    return entry;
  }

  const ReadingListEntry& entry = local_or_syncable_model_->AddOrReplaceEntry(
      url, title, source, estimated_read_time);
  DCHECK(!local_or_syncable_model_->IsTrackingSyncMetadata() ||
         !GetAccountWhereEntryIsSavedTo(url).empty());
  return entry;
}

void DualReadingListModel::RemoveEntryByURL(const GURL& url,
                                            const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
  // If there is no entry with the given URL, then an early return is needed to
  // avoid notifying observers.
  if (!entry) {
    return;
  }

  NotifyObserversWithWillRemoveEntry(url);

  UpdateEntryStateCountersOnEntryRemoval(*entry);

  {
    base::AutoReset<bool> auto_reset_suppress_observer_notifications(
        &suppress_observer_notifications_, true);
    local_or_syncable_model_->RemoveEntryByURL(url, location);
    account_model_->RemoveEntryByURL(url, location);
  }

  NotifyObserversWithDidRemoveEntry(url);
  NotifyObserversWithDidApplyChanges();
}

void DualReadingListModel::SetReadStatusIfExists(const GURL& url, bool read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
  if (!entry) {
    return;
  }

  const bool notify_observers = entry->IsRead() != read;

  if (notify_observers) {
    NotifyObserversWithWillMoveEntry(url);
    UpdateEntryStateCountersOnEntryRemoval(*entry);
  }

  // The update propagates to both underlying ReadingListModelImpl instances
  // even if the `entry` read status is equal to `read`. This is because if
  // `entry` was a merged entry, then one of the two underlying entries may have
  // a different read status.

  {
    base::AutoReset<bool> auto_reset_suppress_observer_notifications(
        &suppress_observer_notifications_, true);
    local_or_syncable_model_->SetReadStatusIfExists(url, read);
    account_model_->SetReadStatusIfExists(url, read);
  }

  if (notify_observers) {
    UpdateEntryStateCountersOnEntryInsertion(*GetEntryByURL(url));
    NotifyObserversWithDidMoveEntry(url);
    NotifyObserversWithDidApplyChanges();
  }
}

void DualReadingListModel::SetEntryTitleIfExists(const GURL& url,
                                                 const std::string& title) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
  if (!entry) {
    return;
  }

  const bool notify_observers =
      entry->Title() != ReadingListModelImpl::TrimTitle(title);

  if (notify_observers) {
    NotifyObserversWithWillUpdateEntry(url);
  }

  // The update propagates to both underlying ReadingListModelImpl instances
  // even if the `entry` title is equal to `title`. This is because if `entry`
  // was a merged entry, then one of the two underlying entries may contain a
  // different title.
  {
    base::AutoReset<bool> auto_reset_suppress_observer_notifications(
        &suppress_observer_notifications_, true);
    local_or_syncable_model_->SetEntryTitleIfExists(url, title);
    account_model_->SetEntryTitleIfExists(url, title);
  }

  if (notify_observers) {
    NotifyObserversWithDidUpdateEntry(url);
    NotifyObserversWithDidApplyChanges();
  }
}

void DualReadingListModel::SetEstimatedReadTimeIfExists(
    const GURL& url,
    base::TimeDelta estimated_read_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
  if (!entry) {
    return;
  }

  const bool notify_observers =
      entry->EstimatedReadTime() != estimated_read_time;

  if (notify_observers) {
    NotifyObserversWithWillUpdateEntry(url);
  }

  // The update propagates to both underlying ReadingListModelImpl instances
  // even if the `entry` estimated read time is equal to `estimated_read_time`.
  // This is because if `entry` was a merged entry, then one of the two
  // underlying entries may have a different estimated read time.
  {
    base::AutoReset<bool> auto_reset_suppress_observer_notifications(
        &suppress_observer_notifications_, true);
    local_or_syncable_model_->SetEstimatedReadTimeIfExists(url,
                                                           estimated_read_time);
    account_model_->SetEstimatedReadTimeIfExists(url, estimated_read_time);
  }

  if (notify_observers) {
    NotifyObserversWithDidUpdateEntry(url);
    NotifyObserversWithDidApplyChanges();
  }
}

void DualReadingListModel::SetEntryDistilledStateIfExists(
    const GURL& url,
    ReadingListEntry::DistillationState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
  if (!entry) {
    return;
  }

  const bool notify_observers = entry->DistilledState() != state;

  if (notify_observers) {
    NotifyObserversWithWillUpdateEntry(url);
  }

  // The update propagates to both underlying ReadingListModelImpl instances
  // even if the `entry` distilled state is equal to `state`. This is because if
  // `entry` was a merged entry, then its distilled state is equal to the local
  // entry's distilled state and the account entry may have a different
  // distilled state.
  {
    base::AutoReset<bool> auto_reset_suppress_observer_notifications(
        &suppress_observer_notifications_, true);
    local_or_syncable_model_->SetEntryDistilledStateIfExists(url, state);
    account_model_->SetEntryDistilledStateIfExists(url, state);
  }

  if (notify_observers) {
    NotifyObserversWithDidUpdateEntry(url);
    NotifyObserversWithDidApplyChanges();
  }
}

void DualReadingListModel::SetEntryDistilledInfoIfExists(
    const GURL& url,
    const base::FilePath& distilled_path,
    const GURL& distilled_url,
    int64_t distilation_size,
    base::Time distilation_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());

  scoped_refptr<const ReadingListEntry> entry = GetEntryByURL(url);
  if (!entry) {
    return;
  }

  const bool notify_observers =
      entry->DistilledState() != ReadingListEntry::PROCESSED ||
      entry->DistilledPath() != distilled_path;

  if (notify_observers) {
    NotifyObserversWithWillUpdateEntry(url);
  }

  // The update propagates to both underlying ReadingListModelImpl instances
  // even if the `entry` distilled path is equal to `distilled_path`. This is
  // because if `entry` was a merged entry, then its distilled path is equal to
  // the local entry's distilled path and the account entry may have a different
  // distilled path.
  {
    base::AutoReset<bool> auto_reset_suppress_observer_notifications(
        &suppress_observer_notifications_, true);
    local_or_syncable_model_->SetEntryDistilledInfoIfExists(
        url, distilled_path, distilled_url, distilation_size, distilation_time);
    account_model_->SetEntryDistilledInfoIfExists(
        url, distilled_path, distilled_url, distilation_size, distilation_time);
  }

  if (notify_observers) {
    NotifyObserversWithDidUpdateEntry(url);
    NotifyObserversWithDidApplyChanges();
  }
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

void DualReadingListModel::RecordCountMetricsOnUMAUpload() const {
  local_or_syncable_model_->RecordCountMetricsOnUMAUpload();
  account_model_->RecordCountMetricsOnUMAUpload();
}

void DualReadingListModel::ReadingListModelBeganBatchUpdates(
    const ReadingListModel* model) {
  DCHECK(!suppress_observer_notifications_);

  if (!loaded()) {
    return;
  }
  ++current_batch_updates_count_;
  if (current_batch_updates_count_ == 1) {
    for (auto& observer : observers_) {
      observer.ReadingListModelBeganBatchUpdates(this);
    }
  }
}

void DualReadingListModel::ReadingListModelCompletedBatchUpdates(
    const ReadingListModel* model) {
  DCHECK(!suppress_observer_notifications_);

  if (!loaded()) {
    return;
  }
  --current_batch_updates_count_;
  if (current_batch_updates_count_ == 0) {
    for (auto& observer : observers_) {
      observer.ReadingListModelCompletedBatchUpdates(this);
    }
  }
}

void DualReadingListModel::ReadingListModelLoaded(
    const ReadingListModel* model) {
  DCHECK(!suppress_observer_notifications_);
  if (!loaded()) {
    return;
  }

  for (const auto& url : GetKeys()) {
    UpdateEntryStateCountersOnEntryInsertion(*GetEntryByURL(url));
  }

  for (auto& observer : observers_) {
    observer.ReadingListModelLoaded(this);
  }
}

void DualReadingListModel::ReadingListWillRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }

  if (local_or_syncable_model_->GetEntryByURL(url) &&
      account_model_->GetEntryByURL(url)) {
    // The fact that the entry exists in one of the models means the result is
    // not an actual deletion but, at most, an update.
    NotifyObserversWithWillUpdateEntry(url);
  } else {
    NotifyObserversWithWillRemoveEntry(url);
  }

  UpdateEntryStateCountersOnEntryRemoval(*GetEntryByURL(url));
}

void DualReadingListModel::ReadingListDidRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }

  if (local_or_syncable_model_->GetEntryByURL(url) ||
      account_model_->GetEntryByURL(url)) {
    // The entry is still present in one of the models, so this is an
    // update rather than a deletion.
    UpdateEntryStateCountersOnEntryInsertion(*GetEntryByURL(url));
    NotifyObserversWithDidUpdateEntry(url);
    return;
  }

  NotifyObserversWithDidRemoveEntry(url);
}

void DualReadingListModel::ReadingListWillMoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }

  NotifyObserversWithWillMoveEntry(url);
  UpdateEntryStateCountersOnEntryRemoval(*GetEntryByURL(url));
}

void DualReadingListModel::ReadingListDidMoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }

  UpdateEntryStateCountersOnEntryInsertion(*GetEntryByURL(url));
  NotifyObserversWithDidMoveEntry(url);
}

void DualReadingListModel::ReadingListWillAddEntry(
    const ReadingListModel* model,
    const ReadingListEntry& entry) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }

  if (local_or_syncable_model_->GetEntryByURL(entry.URL()) ||
      account_model_->GetEntryByURL(entry.URL())) {
    // The presence of the entry in one of the models indicates that this is an
    // update, not an insertion.
    NotifyObserversWithWillUpdateEntry(entry.URL());
    UpdateEntryStateCountersOnEntryRemoval(*GetEntryByURL(entry.URL()));
    return;
  }

  for (auto& observer : observers_) {
    observer.ReadingListWillAddEntry(this, entry);
  }
}

void DualReadingListModel::ReadingListDidAddEntry(
    const ReadingListModel* model,
    const GURL& url,
    reading_list::EntrySource source) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }

  UpdateEntryStateCountersOnEntryInsertion(*GetEntryByURL(url));

  if (local_or_syncable_model_->GetEntryByURL(url) &&
      account_model_->GetEntryByURL(url)) {
    // The entry was added to one of the models, but since it was already
    // present in the other one, then this is an update instead of insertion.
    NotifyObserversWithDidUpdateEntry(url);
    return;
  }

  for (auto& observer : observers_) {
    observer.ReadingListDidAddEntry(this, url, source);
  }
}

void DualReadingListModel::ReadingListWillUpdateEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }
  NotifyObserversWithWillUpdateEntry(url);
}

void DualReadingListModel::ReadingListDidUpdateEntry(
    const ReadingListModel* model,
    const GURL& url) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }
  NotifyObserversWithDidUpdateEntry(url);
}

void DualReadingListModel::ReadingListDidApplyChanges(ReadingListModel* model) {
  if (!loaded() || suppress_observer_notifications_) {
    return;
  }
  NotifyObserversWithDidApplyChanges();
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

void DualReadingListModel::NotifyObserversWithWillMoveEntry(const GURL& url) {
  for (auto& observer : observers_) {
    observer.ReadingListWillMoveEntry(this, url);
  }
}

void DualReadingListModel::NotifyObserversWithDidMoveEntry(const GURL& url) {
  for (auto& observer : observers_) {
    observer.ReadingListDidMoveEntry(this, url);
  }
}

void DualReadingListModel::NotifyObserversWithWillUpdateEntry(const GURL& url) {
  for (auto& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
}

void DualReadingListModel::NotifyObserversWithDidUpdateEntry(const GURL& url) {
  for (auto& observer : observers_) {
    observer.ReadingListDidUpdateEntry(this, url);
  }
}

void DualReadingListModel::NotifyObserversWithDidApplyChanges() {
  for (auto& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

const ReadingListModelImpl* DualReadingListModel::ToReadingListModelImpl(
    const ReadingListModel* model) {
  DCHECK(model == account_model_.get() ||
         model == local_or_syncable_model_.get());

  // It is safe to use static_cast because both `account_model_` and
  // `local_or_syncable_model_` are ReadingListModelImpl, and hence it is also
  // the case for model.
  return static_cast<const ReadingListModelImpl*>(model);
}

void DualReadingListModel::UpdateEntryStateCountersOnEntryRemoval(
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

void DualReadingListModel::UpdateEntryStateCountersOnEntryInsertion(
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

base::flat_set<GURL> DualReadingListModel::GetKeysThatNeedUploadToSyncServer()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!local_or_syncable_model_->IsTrackingSyncMetadata() ||
        !account_model_->IsTrackingSyncMetadata());
  // If `local_or_syncable_model_` is used for sync, no data needs explicit
  // upload to the sync server.
  if (local_or_syncable_model_->IsTrackingSyncMetadata()) {
    return {};
  }
  return local_or_syncable_model_->GetKeys();
}

ReadingListModel* DualReadingListModel::GetLocalOrSyncableModel() {
  return local_or_syncable_model_.get();
}

ReadingListModel* DualReadingListModel::GetAccountModelIfSyncing() {
  return account_model_->IsTrackingSyncMetadata() ? account_model_.get()
                                                  : nullptr;
}

}  // namespace reading_list
