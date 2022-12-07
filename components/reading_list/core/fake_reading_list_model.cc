// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/fake_reading_list_model.h"

#include "base/notreached.h"
#include "components/reading_list/core/reading_list_model_observer.h"

FakeReadingListModel::FakeReadingListModel() = default;

FakeReadingListModel::~FakeReadingListModel() = default;

bool FakeReadingListModel::loaded() const {
  return loaded_;
}

bool FakeReadingListModel::IsPerformingBatchUpdates() const {
  return false;
}

syncer::ModelTypeSyncBridge* FakeReadingListModel::GetModelTypeSyncBridge() {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
FakeReadingListModel::BeginBatchUpdates() {
  NOTREACHED();
  return nullptr;
}

const std::vector<GURL> FakeReadingListModel::Keys() const {
  NOTREACHED();
  return std::vector<GURL>();
}

size_t FakeReadingListModel::size() const {
  DCHECK(loaded_);
  return 0;
}

size_t FakeReadingListModel::unread_size() const {
  NOTREACHED();
  return 0;
}

size_t FakeReadingListModel::unseen_size() const {
  NOTREACHED();
  return 0;
}

void FakeReadingListModel::MarkAllSeen() {
  NOTREACHED();
}

bool FakeReadingListModel::DeleteAllEntries() {
  NOTREACHED();
  return false;
}

const ReadingListEntry* FakeReadingListModel::GetEntryByURL(
    const GURL& gurl) const {
  DCHECK(loaded_);
  DCHECK(entry_);
  if (entry_->URL() == gurl) {
    return &entry_.value();
  }
  return nullptr;
}

const ReadingListEntry* FakeReadingListModel::GetFirstUnreadEntry(
    bool distilled) const {
  NOTREACHED();
  return nullptr;
}

bool FakeReadingListModel::IsUrlSupported(const GURL& url) {
  NOTREACHED();
  return false;
}

const ReadingListEntry& FakeReadingListModel::AddEntry(
    const GURL& url,
    const std::string& title,
    reading_list::EntrySource source) {
  NOTREACHED();
  return *entry_;
}

const ReadingListEntry& FakeReadingListModel::AddEntry(
    const GURL& url,
    const std::string& title,
    reading_list::EntrySource source,
    base::TimeDelta estimated_read_time) {
  NOTREACHED();
  return *entry_;
}

void FakeReadingListModel::RemoveEntryByURL(const GURL& url) {
  NOTREACHED();
}

void FakeReadingListModel::SetReadStatus(const GURL& url, bool read) {
  DCHECK(entry_);
  if (entry_->URL() == url) {
    entry_->SetRead(true, base::Time());
  }
}

void FakeReadingListModel::SetEntryTitle(const GURL& url,
                                         const std::string& title) {
  NOTREACHED();
}

void FakeReadingListModel::SetEntryDistilledState(
    const GURL& url,
    ReadingListEntry::DistillationState state) {
  NOTREACHED();
}

void FakeReadingListModel::SetEstimatedReadTime(
    const GURL& url,
    base::TimeDelta estimated_read_time) {
  NOTREACHED();
}

void FakeReadingListModel::SetEntryDistilledInfo(
    const GURL& url,
    const base::FilePath& distilled_path,
    const GURL& distilled_url,
    int64_t distilation_size,
    const base::Time& distilation_time) {
  NOTREACHED();
}

void FakeReadingListModel::SetContentSuggestionsExtra(
    const GURL& url,
    const reading_list::ContentSuggestionsExtra& extra) {
  NOTREACHED();
}

void FakeReadingListModel::AddObserver(ReadingListModelObserver* observer) {
  observers_.AddObserver(observer);
  if (loaded()) {
    observer->ReadingListModelLoaded(this);
  }
}

void FakeReadingListModel::RemoveObserver(ReadingListModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FakeReadingListModel::SetEntry(ReadingListEntry entry) {
  entry_ = std::move(entry);
}

void FakeReadingListModel::SetLoaded() {
  loaded_ = true;
  for (auto& observer : observers_) {
    observer.ReadingListModelLoaded(this);
  }
}

const ReadingListEntry* FakeReadingListModel::entry() {
  return entry_ ? &entry_.value() : nullptr;
}
