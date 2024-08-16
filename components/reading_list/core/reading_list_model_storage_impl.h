// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_IMPL_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/reading_list/core/reading_list_model_storage.h"
#include "components/sync/model/data_type_store.h"

namespace base {
class Clock;
}  // namespace base

// A ReadingListModelStorage storing data in protobufs within DataTypeStore
// (leveldb).
class ReadingListModelStorageImpl : public ReadingListModelStorage {
 public:
  explicit ReadingListModelStorageImpl(
      syncer::OnceDataTypeStoreFactory create_store_callback);

  ReadingListModelStorageImpl(const ReadingListModelStorageImpl&) = delete;
  ReadingListModelStorageImpl& operator=(const ReadingListModelStorageImpl&) =
      delete;

  ~ReadingListModelStorageImpl() override;

  std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() override;
  void DeleteAllEntriesAndSyncMetadata() override;

  // ReadingListModelStorage implementation.
  void Load(base::Clock* clock, LoadCallback load_cb) override;

  class ScopedBatchUpdate : public ReadingListModelStorage::ScopedBatchUpdate {
   public:
    explicit ScopedBatchUpdate(ReadingListModelStorageImpl* store);

    ScopedBatchUpdate(const ScopedBatchUpdate&) = delete;
    ScopedBatchUpdate& operator=(const ScopedBatchUpdate&) = delete;

    ~ScopedBatchUpdate() override;

    void SaveEntry(const ReadingListEntry& entry) override;
    void RemoveEntry(const GURL& entry_url) override;
    syncer::MetadataChangeList* GetSyncMetadataChangeList() override;

   private:
    const raw_ptr<ReadingListModelStorageImpl> store_;
    SEQUENCE_CHECKER(sequence_checker_);
  };

 private:
  void BeginTransaction();
  void CommitTransaction();
  // Callbacks needed for the database handling.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnDatabaseLoad(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries);
  void OnDatabaseSave(const std::optional<syncer::ModelError>& error);
  void OnReadAllMetadata(ReadingListEntries loaded_entries,
                         const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  bool loaded_ = false;
  std::unique_ptr<syncer::DataTypeStore> store_;
  syncer::OnceDataTypeStoreFactory create_store_callback_;
  LoadCallback load_callback_;

  int pending_transaction_count_ = 0;
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch_;

  raw_ptr<base::Clock> clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ReadingListModelStorageImpl> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_MODEL_STORAGE_IMPL_H_
