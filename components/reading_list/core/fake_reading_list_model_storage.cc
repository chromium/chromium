// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/fake_reading_list_model_storage.h"

FakeReadingListModelStorage::FakeReadingListModelStorage()
    : FakeReadingListModelStorage(/*observer=*/nullptr) {}

FakeReadingListModelStorage::FakeReadingListModelStorage(Observer* observer)
    : observer_(observer) {}

FakeReadingListModelStorage::~FakeReadingListModelStorage() = default;

bool FakeReadingListModelStorage::TriggerLoadCompletion(
    LoadResultOrError load_result_or_error) {
  if (!load_callback_) {
    return false;
  }

  std::move(load_callback_).Run(std::move(load_result_or_error));
  return true;
}

bool FakeReadingListModelStorage::TriggerLoadCompletion() {
  return TriggerLoadCompletion(LoadResult());
}

void FakeReadingListModelStorage::Load(LoadCallback load_cb) {
  load_callback_ = std::move(load_cb);
}

std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate>
FakeReadingListModelStorage::EnsureBatchCreated() {
  return nullptr;
}

void FakeReadingListModelStorage::SaveEntry(const ReadingListEntry& entry) {
  if (observer_) {
    observer_->FakeStorageDidSaveEntry();
  }
}

void FakeReadingListModelStorage::RemoveEntry(const ReadingListEntry& entry) {
  if (observer_) {
    observer_->FakeStorageDidRemoveEntry();
  }
}

ReadingListSyncBridge* FakeReadingListModelStorage::GetSyncBridge() {
  return nullptr;
}
