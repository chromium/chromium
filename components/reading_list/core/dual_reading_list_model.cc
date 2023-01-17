// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/dual_reading_list_model.h"

#include "base/notreached.h"
#include "base/stl_util.h"
#include "components/reading_list/features/reading_list_switches.h"

namespace reading_list {

DualReadingListModel::DualReadingListModel(
    std::unique_ptr<ReadingListModel> local_or_syncable_model,
    std::unique_ptr<ReadingListModel> account_model)
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
  return local_or_syncable_model_->IsPerformingBatchUpdates() ||
         account_model_->IsPerformingBatchUpdates();
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
DualReadingListModel::BeginBatchUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
  return nullptr;
}

base::flat_set<GURL> DualReadingListModel::GetKeys() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::STLSetUnion<base::flat_set<GURL>>(
      local_or_syncable_model_->GetKeys(), account_model_->GetKeys());
}

size_t DualReadingListModel::size() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement more efficiently.
  NOTIMPLEMENTED();
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

const ReadingListEntry* DualReadingListModel::GetEntryByURL(
    const GURL& gurl) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
  return nullptr;
}

bool DualReadingListModel::IsUrlSupported(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(local_or_syncable_model_->IsUrlSupported(url),
            account_model_->IsUrlSupported(url));
  return local_or_syncable_model_->IsUrlSupported(url);
}

const ReadingListEntry& DualReadingListModel::AddOrReplaceEntry(
    const GURL& url,
    const std::string& title,
    reading_list::EntrySource source,
    base::TimeDelta estimated_read_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded());
  DCHECK(IsUrlSupported(url));

  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
  return local_or_syncable_model_->AddOrReplaceEntry(url, title, source,
                                                     estimated_read_time);
}

void DualReadingListModel::RemoveEntryByURL(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1402196): Implement.
  NOTIMPLEMENTED();
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

void DualReadingListModel::ReadingListModelLoaded(
    const ReadingListModel* model) {
  if (loaded()) {
    for (auto& observer : observers_) {
      observer.ReadingListModelLoaded(this);
    }
  }
}

}  // namespace reading_list
