// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "url/gurl.h"

ReadingListModelImpl::ReadingListModelImpl(
    std::unique_ptr<ReadingListModelStorage> storage,
    PrefService* pref_service,
    base::Clock* clock)
    : entries_(std::make_unique<ReadingListEntries>()),
      unread_entry_count_(0),
      read_entry_count_(0),
      unseen_entry_count_(0),
      clock_(clock),
      pref_service_(pref_service),
      has_unseen_(false),
      loaded_(false) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(clock_);
  if (storage) {
    storage_layer_ = std::move(storage);
    storage_layer_->SetReadingListModel(this, this, clock_);
  } else {
    loaded_ = true;
  }
  has_unseen_ = GetPersistentHasUnseen();
}

ReadingListModelImpl::~ReadingListModelImpl() {}

void ReadingListModelImpl::StoreLoaded(
    std::unique_ptr<ReadingListEntries> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entries);
  entries_ = std::move(entries);
  for (auto& iterator : *entries_) {
    UpdateEntryStateCountersOnEntryInsertion(iterator.second);
  }
  DCHECK(read_entry_count_ + unread_entry_count_ == entries_->size());
  loaded_ = true;
  for (auto& observer : observers_)
    observer.ReadingListModelLoaded(this);
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

size_t ReadingListModelImpl::size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_entry_count_ + unread_entry_count_ == entries_->size());
  if (!loaded())
    return 0;
  return entries_->size();
}

size_t ReadingListModelImpl::unread_size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(read_entry_count_ + unread_entry_count_ == entries_->size());
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

void ReadingListModelImpl::SetUnseenFlag() {
  if (!has_unseen_) {
    has_unseen_ = true;
    if (!IsPerformingBatchUpdates()) {
      SetPersistentHasUnseen(true);
    }
  }
}

bool ReadingListModelImpl::GetLocalUnseenFlag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loaded())
    return false;
  // If there are currently no unseen entries, return false even if has_unseen_
  // is true.
  // This is possible if the last unseen entry has be removed via sync.
  return has_unseen_ && unseen_entry_count_;
}

void ReadingListModelImpl::ResetLocalUnseenFlag() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!loaded()) {
    return;
  }
  has_unseen_ = false;
  if (!IsPerformingBatchUpdates())
    SetPersistentHasUnseen(false);
}

void ReadingListModelImpl::MarkAllSeen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  if (unseen_entry_count_ == 0) {
    return;
  }
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
      model_batch_updates = BeginBatchUpdates();
  for (auto& iterator : *entries_) {
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
      storage_layer_->SaveEntry(entry);
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
  return entries_->empty();
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
  for (const auto& iterator : *entries_) {
    keys.push_back(iterator.first);
  }
  return keys;
}

const ReadingListEntry* ReadingListModelImpl::GetEntryByURL(
    const GURL& gurl) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  return GetMutableEntryFromURL(gurl);
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
  for (auto& iterator : *entries_) {
    ReadingListEntry& entry = iterator.second;
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
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
    return nullptr;
  }
  return &(iterator->second);
}

void ReadingListModelImpl::SyncAddEntry(
    std::unique_ptr<ReadingListEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  // entry must not already exist.
  DCHECK(GetMutableEntryFromURL(entry->URL()) == nullptr);
  for (auto& observer : observers_)
    observer.ReadingListWillAddEntry(this, *entry);
  UpdateEntryStateCountersOnEntryInsertion(*entry);
  if (!entry->HasBeenSeen()) {
    SetUnseenFlag();
  }
  GURL url = entry->URL();
  entries_->insert(std::make_pair(url, std::move(*entry)));
  for (auto& observer : observers_) {
    observer.ReadingListDidAddEntry(this, url, reading_list::ADDED_VIA_SYNC);
    observer.ReadingListDidApplyChanges(this);
  }
}

ReadingListEntry* ReadingListModelImpl::SyncMergeEntry(
    std::unique_ptr<ReadingListEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  ReadingListEntry* existing_entry = GetMutableEntryFromURL(entry->URL());
  DCHECK(existing_entry);
  GURL url = entry->URL();

  for (auto& observer : observers_)
    observer.ReadingListWillMoveEntry(this, url);

  bool was_seen = existing_entry->HasBeenSeen();
  UpdateEntryStateCountersOnEntryRemoval(*existing_entry);
  existing_entry->MergeWithEntry(*entry);
  existing_entry = GetMutableEntryFromURL(url);
  UpdateEntryStateCountersOnEntryInsertion(*existing_entry);
  if (was_seen && !existing_entry->HasBeenSeen()) {
    // Only set the flag if a new unseen entry is added.
    SetUnseenFlag();
  }
  for (auto& observer : observers_) {
    observer.ReadingListDidMoveEntry(this, url);
    observer.ReadingListDidApplyChanges(this);
  }

  return existing_entry;
}

void ReadingListModelImpl::SyncRemoveEntry(const GURL& url) {
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

  if (storage_layer_ && !from_sync) {
    storage_layer_->RemoveEntry(*entry);
  }
  UpdateEntryStateCountersOnEntryRemoval(*entry);

  entries_->erase(url);
  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);
}

const ReadingListEntry& ReadingListModelImpl::AddEntry(
    const GURL& url,
    const std::string& title,
    reading_list::EntrySource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
      scoped_model_batch_updates = nullptr;
  if (GetEntryByURL(url)) {
    scoped_model_batch_updates = BeginBatchUpdates();
    RemoveEntryByURL(url);
  }

  std::string trimmed_title = base::CollapseWhitespaceASCII(title, false);

  ReadingListEntry entry(url, trimmed_title, clock_->Now());
  for (auto& observer : observers_)
    observer.ReadingListWillAddEntry(this, entry);
  UpdateEntryStateCountersOnEntryInsertion(entry);
  SetUnseenFlag();
  entries_->insert(std::make_pair(url, std::move(entry)));

  if (storage_layer_) {
    storage_layer_->SaveEntry(*GetEntryByURL(url));
  }

  for (auto& observer : observers_) {
    observer.ReadingListDidAddEntry(this, url, source);
    observer.ReadingListDidApplyChanges(this);
  }

  return entries_->at(url);
}

void ReadingListModelImpl::SetReadStatus(const GURL& url, bool read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
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
    storage_layer_->SaveEntry(entry);
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
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
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
    storage_layer_->SaveEntry(entry);
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
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
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
    storage_layer_->SaveEntry(entry);
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
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
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
    storage_layer_->SaveEntry(entry);
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
    storage_layer_->SaveEntry(*entry);
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
ReadingListModelImpl::CreateBatchToken() {
  return std::make_unique<ReadingListModelImpl::ScopedReadingListBatchUpdate>(
      this);
}

ReadingListModelImpl::ScopedReadingListBatchUpdate::
    ScopedReadingListBatchUpdate(ReadingListModelImpl* model)
    : ReadingListModel::ScopedReadingListBatchUpdate::
          ScopedReadingListBatchUpdate(model) {
  if (model->StorageLayer()) {
    storage_token_ = model->StorageLayer()->EnsureBatchCreated();
  }
}

ReadingListModelImpl::ScopedReadingListBatchUpdate::
    ~ScopedReadingListBatchUpdate() {
  storage_token_.reset();
}

void ReadingListModelImpl::LeavingBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (storage_layer_) {
    SetPersistentHasUnseen(has_unseen_);
  }
  ReadingListModel::LeavingBatchUpdates();
}

void ReadingListModelImpl::EnteringBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ReadingListModel::EnteringBatchUpdates();
}

void ReadingListModelImpl::SetPersistentHasUnseen(bool has_unseen) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return;
  }
  pref_service_->SetBoolean(reading_list::prefs::kReadingListHasUnseenEntries,
                            has_unseen);
}

bool ReadingListModelImpl::GetPersistentHasUnseen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return false;
  }
  return pref_service_->GetBoolean(
      reading_list::prefs::kReadingListHasUnseenEntries);
}

syncer::ModelTypeSyncBridge* ReadingListModelImpl::GetModelTypeSyncBridge() {
  if (!storage_layer_)
    return nullptr;
  return storage_layer_.get();
}

ReadingListModelStorage* ReadingListModelImpl::StorageLayer() {
  return storage_layer_.get();
}
