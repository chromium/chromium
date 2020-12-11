// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_STORE_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_STORE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/reading_list/core/reading_list_model_storage.h"
#include "components/reading_list/core/reading_list_store_delegate.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {
class MutableDataBatch;
}

class ReadingListModel;

// A ReadingListModelStorage storing and syncing data in protobufs.
class ReadingListStore : public ReadingListModelStorage {
 public:
  ReadingListStore(
      syncer::OnceModelTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
  ~ReadingListStore() override;

  std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() override;

  // ReadingListModelStorage implementation
  void SetReadingListModel(ReadingListModel* model,
                           ReadingListStoreDelegate* delegate,
                           base::Clock* clock) override;

  void SaveEntry(const ReadingListEntry& entry) override;
  void RemoveEntry(const ReadingListEntry& entry) override;

  // Creates an object used to communicate changes in the sync metadata to the
  // model type store.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;

  // Perform the initial merge between local and sync data. This should only be
  // called when a data type is first enabled to start syncing, and there is no
  // sync metadata. Best effort should be made to match local and sync data. The
  // storage keys in the |entity_data| are populated with GetStorageKey(...),
  // local and sync copies of the same entity should resolve to the same storage
  // key. Any local pieces of data that are not present in sync should
  // immediately be Put(...) to the processor before returning. The same
  // MetadataChangeList that was passed into this function can be passed to
  // Put(...) calls. Delete(...) can also be called but should not be needed for
  // most model types. Durable storage writes, if not able to combine all change
  // atomically, should save the metadata after the data changes, so that this
  // merge will be re-driven by sync if is not completely saved during the
  // current run.
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;

  // Apply changes from the sync server locally.
  // Please note that |entity_changes| might have fewer entries than
  // |metadata_change_list| in case when some of the data changes are filtered
  // out, or even be empty in case when a commit confirmation is processed and
  // only the metadata needs to persisted.
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;

  // Returns whether entries respect a strict order for sync and if |rhs| can be
  // submitted to sync after |lhs| has been received.
  // The order should ensure that there is no sync loop in sync and should be
  // submitted to sync in strictly increasing order.
  // Entries are in increasing order if all the fields respect increasing order.
  // - URL must be the same.
  // - update_title_time_us:
  //       rhs.update_title_time_us >= lhs.update_title_time_us
  // - title:
  //       if rhs.update_title_time_us > lhs.update_title_time_us
  //         title can be anything
  //       if rhs.update_title_time_us == lhs.update_title_time_us
  //         title must verify rhs.title.compare(lhs.title) >= 0
  // - creation_time_us:
  //       rhs.creation_time_us >= lhs.creation_time_us
  // - rhs.first_read_time_us:
  //       if rhs.creation_time_us > lhs.creation_time_us,
  //         rhs.first_read_time_us can be anything.
  //       if rhs.creation_time_us == lhs.creation_time_us
  //           and rhs.first_read_time_us == 0
  //         rhs.first_read_time_us can be anything.
  //       if rhs.creation_time_us == lhs.creation_time_us,
  //         rhs.first_read_time_us <= lhs.first_read_time_us
  // - update_time_us:
  //       rhs.update_time_us >= lhs.update_time_us
  // - state:
  //       if rhs.update_time_us > lhs.update_time_us
  //         rhs.state can be anything.
  //       if rhs.update_time_us == lhs.update_time_us
  //         rhs.state >= lhs.state in the order UNSEEN, UNREAD, READ.
  static bool CompareEntriesForSync(const sync_pb::ReadingListSpecifics& lhs,
                                    const sync_pb::ReadingListSpecifics& rhs);

  // Asynchronously retrieve the corresponding sync data for |storage_keys|.
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;

  // Asynchronously retrieve all of the local sync data.
  void GetAllDataForDebugging(DataCallback callback) override;

  // Get or generate a client tag for |entity_data|. This must be the same tag
  // that was/would have been generated in the SyncableService/Directory world
  // for backward compatibility with pre-USS clients. The only time this
  // theoretically needs to be called is on the creation of local data, however
  // it is also used to verify the hash of remote data. If a data type was never
  // launched pre-USS, then method does not need to be different from
  // GetStorageKey().
  std::string GetClientTag(const syncer::EntityData& entity_data) override;

  // Get or generate a storage key for |entity_data|. This will only ever be
  // called once when first encountering a remote entity. Local changes will
  // provide their storage keys directly to Put instead of using this method.
  // Theoretically this function doesn't need to be stable across multiple calls
  // on the same or different clients, but to keep things simple, it probably
  // should be.
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // Methods used as callbacks given to DataTypeStore.
  void OnStoreCreated(const base::Optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  class ScopedBatchUpdate : public ReadingListModelStorage::ScopedBatchUpdate {
   public:
    explicit ScopedBatchUpdate(ReadingListStore* store);

    ~ScopedBatchUpdate() override;

   private:
    ReadingListStore* store_;

    DISALLOW_COPY_AND_ASSIGN(ScopedBatchUpdate);
  };

 private:
  void BeginTransaction();
  void CommitTransaction();
  // Callbacks needed for the database handling.
  void OnDatabaseLoad(
      const base::Optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries);
  void OnDatabaseSave(const base::Optional<syncer::ModelError>& error);
  void OnReadAllMetadata(const base::Optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void AddEntryToBatch(syncer::MutableDataBatch* batch,
                       const ReadingListEntry& entry);

  std::unique_ptr<syncer::ModelTypeStore> store_;
  ReadingListModel* model_;
  ReadingListStoreDelegate* delegate_;
  syncer::OnceModelTypeStoreFactory create_store_callback_;

  int pending_transaction_count_;
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch_;

  base::Clock* clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ReadingListStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReadingListStore);
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_STORE_H_
