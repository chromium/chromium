// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_FAKE_READING_LIST_MODEL_STORAGE_H_
#define COMPONENTS_READING_LIST_CORE_FAKE_READING_LIST_MODEL_STORAGE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/reading_list/core/reading_list_model_storage.h"

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

  FakeReadingListModelStorage();
  explicit FakeReadingListModelStorage(Observer* observer);
  ~FakeReadingListModelStorage() override;

  // By default the Load() operation never completes. Tests need to invoke this
  // function explicitly to mimic completion. Returns false in case no actual
  // load operation was ongoing.
  bool TriggerLoadCompletion(LoadResultOrError load_result_or_error);

  // Convenience overload that uses the default (empty-store) success case.
  bool TriggerLoadCompletion();

  // ReadingListModelStorage implementation.
  void Load(LoadCallback load_cb) override;
  std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() override;
  void SaveEntry(const ReadingListEntry& entry) override;
  void RemoveEntry(const ReadingListEntry& entry) override;
  ReadingListSyncBridge* GetSyncBridge() override;

 private:
  const raw_ptr<Observer> observer_ = nullptr;
  LoadCallback load_callback_;
};

#endif  // COMPONENTS_READING_LIST_CORE_FAKE_READING_LIST_MODEL_STORAGE_H_
