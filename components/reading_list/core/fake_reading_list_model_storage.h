// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_FAKE_READING_LIST_MODEL_STORAGE_H_
#define COMPONENTS_READING_LIST_CORE_FAKE_READING_LIST_MODEL_STORAGE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_storage.h"
#include "components/sync/model/dummy_metadata_change_list.h"

// Test-only implementation of ReadingListModelStorage that doesn't do any
// actual I/O but allows populating the initial list of entries. It also
// allows tests to observe calls to I/O operations via observer.
class FakeReadingListModelStorage
    : public ReadingListModelStorage,
      public base::SupportsWeakPtr<FakeReadingListModelStorage> {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;
    virtual void FakeStorageDidSaveEntry() = 0;
    virtual void FakeStorageDidRemoveEntry() = 0;
  };

  class FakeScopedBatchUpdate : public ScopedBatchUpdate {
   public:
    explicit FakeScopedBatchUpdate(Observer* observer);

    FakeScopedBatchUpdate(const FakeScopedBatchUpdate&) = delete;
    FakeScopedBatchUpdate& operator=(const FakeScopedBatchUpdate&) = delete;

    ~FakeScopedBatchUpdate() override;

    void SaveEntry(const ReadingListEntry& entry) override;
    void RemoveEntry(const GURL& entry_url) override;
    syncer::MetadataChangeList* GetSyncMetadataChangeList() override;

   private:
    const raw_ptr<Observer> observer_ = nullptr;
    syncer::DummyMetadataChangeList sync_metadata_change_list;
  };

  FakeReadingListModelStorage();
  explicit FakeReadingListModelStorage(Observer* observer);
  ~FakeReadingListModelStorage() override;

  // By default the Load() operation never completes. Tests need to invoke this
  // function explicitly to mimic completion. Returns false in case no actual
  // load operation was ongoing.
  bool TriggerLoadCompletion(LoadResultOrError load_result_or_error);

  // Convenience overload that uses sensible defaults (empty store) for success
  // case.
  bool TriggerLoadCompletion(std::vector<ReadingListEntry> entries = {});

  // ReadingListModelStorage implementation.
  void Load(base::Clock* clock, LoadCallback load_cb) override;
  std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() override;

 private:
  const raw_ptr<Observer> observer_ = nullptr;
  LoadCallback load_callback_;
};

#endif  // COMPONENTS_READING_LIST_CORE_FAKE_READING_LIST_MODEL_STORAGE_H_
