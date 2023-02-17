// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/fake_reading_list_model_storage.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/sync/model/metadata_batch.h"

FakeReadingListModelStorage::FakeScopedBatchUpdate::FakeScopedBatchUpdate(
    Observer* observer)
    : observer_(observer) {}

FakeReadingListModelStorage::FakeScopedBatchUpdate::~FakeScopedBatchUpdate() =
    default;

void FakeReadingListModelStorage::FakeScopedBatchUpdate::SaveEntry(
    const ReadingListEntry& entry) {
  if (observer_) {
    observer_->FakeStorageDidSaveEntry();
  }
}

void FakeReadingListModelStorage::FakeScopedBatchUpdate::RemoveEntry(
    const GURL& entry_url) {
  if (observer_) {
    observer_->FakeStorageDidRemoveEntry();
  }
}

syncer::MetadataChangeList* FakeReadingListModelStorage::FakeScopedBatchUpdate::
    GetSyncMetadataChangeList() {
  return &sync_metadata_change_list;
}

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

bool FakeReadingListModelStorage::TriggerLoadCompletion(
    std::vector<scoped_refptr<ReadingListEntry>> entries,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  LoadResult result;
  result.second = std::move(metadata_batch);
  for (auto& entry : entries) {
    GURL url = entry->URL();
    result.first.emplace(entry->URL(), std::move(entry));
  }
  return TriggerLoadCompletion(std::move(result));
}

void FakeReadingListModelStorage::Load(base::Clock* clock,
                                       LoadCallback load_cb) {
  load_callback_ = std::move(load_cb);
}

std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate>
FakeReadingListModelStorage::EnsureBatchCreated() {
  return std::make_unique<FakeScopedBatchUpdate>(observer_);
}

void FakeReadingListModelStorage::DeleteAllEntriesAndSyncMetadata() {}
