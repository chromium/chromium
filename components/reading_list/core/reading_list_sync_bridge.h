// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_SYNC_BRIDGE_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"

namespace base {
class Clock;
class Location;
}  // namespace base

namespace syncer {
class DataTypeLocalChangeProcessor;
class MetadataChangeList;
class MutableDataBatch;
}  // namespace syncer

class ReadingListEntry;
class ReadingListModelImpl;

// Sync bridge implementation for READING_LIST data type. Takes care of
// propagating local passwords to other clients and vice versa.
class ReadingListSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  ReadingListSyncBridge(
      syncer::StorageType storage_type,
      syncer::WipeModelUponSyncDisabledBehavior
          wipe_model_upon_sync_disabled_behavior,
      base::Clock* clock,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  ReadingListSyncBridge(const ReadingListSyncBridge&) = delete;
  ReadingListSyncBridge& operator=(const ReadingListSyncBridge&) = delete;

  ~ReadingListSyncBridge() override;

  void ModelReadyToSync(
      ReadingListModelImpl* model,
      std::unique_ptr<syncer::MetadataBatch> sync_metadata_batch);
  void ReportError(const syncer::ModelError& error);

  // Observer-like functions that explicitly pass syncer::MetadataChangeList.
  void DidAddOrUpdateEntry(const ReadingListEntry& entry,
                           syncer::MetadataChangeList* metadata_change_list);
  void DidRemoveEntry(const ReadingListEntry& entry,
                      const base::Location& location,
                      syncer::MetadataChangeList* metadata_change_list);

  // Exposes whether the underlying DataTypeLocalChangeProcessor is tracking
  // metadata. This means sync is enabled and the initial download of data is
  // completed, which implies that the relevant ReadingListModel already
  // reflects remote data. Note however that this doesn't mean reading list
  // entries are actively sync-ing at the moment, for example sync could be
  // paused due to an auth error.
  bool IsTrackingMetadata() const;

  // Returns the StorageType, as passed to the constructor.
  syncer::StorageType GetStorageTypeForUma() const;

  // Creates an object used to communicate changes in the sync metadata to the
  // data type store.
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
  // most data types. Durable storage writes, if not able to combine all change
  // atomically, should save the metadata after the data changes, so that this
  // merge will be re-driven by sync if is not completely saved during the
  // current run.
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;

  // Apply changes from the sync server locally.
  // Please note that |entity_changes| might have fewer entries than
  // |metadata_change_list| in case when some of the data changes are filtered
  // out, or even be empty in case when a commit confirmation is processed and
  // only the metadata needs to persisted.
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
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

  // Retrieve the corresponding sync data for |storage_keys|.
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;

  // Retrieve all of the local sync data.
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;

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

  // Invoked when sync is permanently stopped.
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

 private:
  void AddEntryToBatch(syncer::MutableDataBatch* batch,
                       const ReadingListEntry& entry);

  // Only true when ApplyDisableSyncChanges() is running, false otherwise.
  bool ongoing_apply_disable_sync_changes_ = false;
  const syncer::StorageType storage_type_for_uma_;
  const raw_ptr<base::Clock> clock_;
  raw_ptr<ReadingListModelImpl> model_ = nullptr;
  syncer::WipeModelUponSyncDisabledBehavior
      wipe_model_upon_sync_disabled_behavior_ =
          syncer::WipeModelUponSyncDisabledBehavior::kNever;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_SYNC_BRIDGE_H_
