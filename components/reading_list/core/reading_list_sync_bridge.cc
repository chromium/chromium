// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_sync_bridge.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/clock.h"
#include "base/trace_event/trace_event.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/data_type_state_helper.h"

ReadingListSyncBridge::ReadingListSyncBridge(
    syncer::StorageType storage_type,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior,
    base::Clock* clock,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : DataTypeSyncBridge(std::move(change_processor)),
      storage_type_for_uma_(storage_type),
      clock_(clock),
      wipe_model_upon_sync_disabled_behavior_(
          wipe_model_upon_sync_disabled_behavior) {}

ReadingListSyncBridge::~ReadingListSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ReadingListSyncBridge::ModelReadyToSync(
    ReadingListModelImpl* model,
    std::unique_ptr<syncer::MetadataBatch> sync_metadata_batch) {
  TRACE_EVENT0("ui", "ReadingListSyncBridge::ModelReadyToSync");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model);
  DCHECK(!model_);

  model_ = model;

  if (wipe_model_upon_sync_disabled_behavior_ ==
          syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata &&
      !syncer::IsInitialSyncDone(
          sync_metadata_batch->GetDataTypeState().initial_sync_state())) {
    // Since the model isn't initially tracking metadata, move away from
    // kOnceIfTrackingMetadata so the behavior doesn't kick in, in case sync is
    // turned on later and back to off.
    //
    // Note that implementing this using IsInitialSyncDone(), instead of
    // invoking IsTrackingMetadata() later, is more reliable, because the
    // function cannot be trusted in ApplyDisableSyncChanges(), as it can
    // return false negatives.
    wipe_model_upon_sync_disabled_behavior_ =
        syncer::WipeModelUponSyncDisabledBehavior::kNever;
  }

  change_processor()->ModelReadyToSync(std::move(sync_metadata_batch));
}

void ReadingListSyncBridge::ReportError(const syncer::ModelError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  change_processor()->ReportError(error);
}

void ReadingListSyncBridge::DidAddOrUpdateEntry(
    const ReadingListEntry& entry,
    syncer::MetadataChangeList* metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::unique_ptr<sync_pb::ReadingListSpecifics> pb_entry_sync =
      entry.AsReadingListSpecifics();

  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_reading_list() = *pb_entry_sync;
  entity_data->name = pb_entry_sync->entry_id();

  change_processor()->Put(entry.URL().spec(), std::move(entity_data),
                          metadata_change_list);
}

void ReadingListSyncBridge::DidRemoveEntry(
    const ReadingListEntry& entry,
    const base::Location& location,
    syncer::MetadataChangeList* metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  change_processor()->Delete(entry.URL().spec(),
                             syncer::DeletionOrigin::FromLocation(location),
                             metadata_change_list);
}

// IsTrackingMetadata() continues to be true while ApplyDisableSyncChanges() is
// running, but transitions to false immediately afterwards.
// ongoing_apply_disable_sync_changes_ is used to cause IsTrackingMetadata()
// return false slightly earlier, and before related observer notifications are
// triggered.
bool ReadingListSyncBridge::IsTrackingMetadata() const {
  return !ongoing_apply_disable_sync_changes_ &&
         change_processor()->IsTrackingMetadata();
}

syncer::StorageType ReadingListSyncBridge::GetStorageTypeForUma() const {
  return storage_type_for_uma_;
}

std::unique_ptr<syncer::MetadataChangeList>
ReadingListSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

// Perform the initial merge between local and sync data. This should only be
// called when a data type is first enabled to start syncing, and there is no
// sync metadata. Best effort should be made to match local and sync data. The
// storage keys in the |entity_data| are populated with GetStorageKey(...),
// local and sync copies of the same entity should resolve to the same storage
// key. Any local pieces of data that are not present in sync should immediately
// be Put(...) to the processor before returning. The same MetadataChangeList
// that was passed into this function can be passed to Put(...) calls.
// Delete(...) can also be called but should not be needed for most data types.
// Durable storage writes, if not able to combine all change atomically, should
// save the metadata after the data changes, so that this merge will be re-
// driven by sync if is not completely saved during the current run.
std::optional<syncer::ModelError> ReadingListSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_);

  // Keep track of the last update of each item.
  std::set<std::string> synced_entries;
  std::unique_ptr<ReadingListModelImpl::ScopedReadingListBatchUpdateImpl>
      model_batch_updates = model_->BeginBatchUpdatesWithSyncMetadata();

  // Merge sync to local data.
  for (const auto& change : entity_changes) {
    synced_entries.insert(change->storage_key());
    const sync_pb::ReadingListSpecifics& specifics =
        change->data().specifics.reading_list();

    // The specifics validity is guaranteed by IsEntityDataValid().
    CHECK(ReadingListEntry::IsSpecificsValid(specifics));
    // Deserialize entry.
    scoped_refptr<ReadingListEntry> entry(
        ReadingListEntry::FromReadingListValidSpecifics(specifics,
                                                        clock_->Now()));

    scoped_refptr<const ReadingListEntry> existing_entry =
        model_->GetEntryByURL(entry->URL());

    if (!existing_entry) {
      model_->AddEntry(std::move(entry), reading_list::ADDED_VIA_SYNC);
    } else {
      ReadingListEntry* merged_entry = model_->SyncMergeEntry(std::move(entry));

      // Send to sync
      std::unique_ptr<sync_pb::ReadingListSpecifics> entry_sync_pb =
          merged_entry->AsReadingListSpecifics();
#if !defined(NDEBUG)
      scoped_refptr<ReadingListEntry> initial_entry(
          ReadingListEntry::FromReadingListValidSpecifics(specifics,
                                                          clock_->Now()));
      DCHECK(CompareEntriesForSync(*(initial_entry->AsReadingListSpecifics()),
                                   *entry_sync_pb));
#endif
      auto entity_data = std::make_unique<syncer::EntityData>();
      *(entity_data->specifics.mutable_reading_list()) = *entry_sync_pb;
      entity_data->name = entry_sync_pb->entry_id();

      change_processor()->Put(entry_sync_pb->entry_id(), std::move(entity_data),
                              metadata_change_list.get());
    }
  }

  // Commit local only entries to server.
  for (const auto& url : model_->GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry = model_->GetEntryByURL(url);
    if (synced_entries.count(url.spec())) {
      // Entry already exists and has been merged above.
      continue;
    }

    // Local entry has later timestamp. It should be committed to server.
    std::unique_ptr<sync_pb::ReadingListSpecifics> entry_pb =
        entry->AsReadingListSpecifics();

    auto entity_data = std::make_unique<syncer::EntityData>();
    *(entity_data->specifics.mutable_reading_list()) = *entry_pb;
    entity_data->name = entry_pb->entry_id();

    change_processor()->Put(entry_pb->entry_id(), std::move(entity_data),
                            metadata_change_list.get());
  }

  static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
      ->TransferChangesTo(model_batch_updates->GetSyncMetadataChangeList());

  return {};
}

// Apply changes from the sync server locally.
// Please note that |entity_changes| might have fewer entries than
// |metadata_change_list| in case when some of the data changes are filtered
// out, or even be empty in case when a commit confirmation is processed and
// only the metadata needs to persisted.
std::optional<syncer::ModelError>
ReadingListSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_);

  std::unique_ptr<ReadingListModelImpl::ScopedReadingListBatchUpdateImpl>
      model_batch_updates = model_->BeginBatchUpdatesWithSyncMetadata();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      model_->SyncRemoveEntry(GURL(change->storage_key()));
    } else {
      // Deserialize entry.
      const sync_pb::ReadingListSpecifics& specifics =
          change->data().specifics.reading_list();

      // The specifics validity is guaranteed by IsEntityDataValid().
      CHECK(ReadingListEntry::IsSpecificsValid(specifics));

      scoped_refptr<ReadingListEntry> entry(
          ReadingListEntry::FromReadingListValidSpecifics(specifics,
                                                          clock_->Now()));

      scoped_refptr<const ReadingListEntry> existing_entry =
          model_->GetEntryByURL(entry->URL());

      if (!existing_entry) {
        model_->AddEntry(std::move(entry), reading_list::ADDED_VIA_SYNC);
      } else {
        // Merge the local data and the sync data and store the result.
        model_->SyncMergeEntry(std::move(entry));

        // Note: Do NOT send the merged data back to Sync. Doing that could
        // cause ping-pong between two devices that disagree on the "correct"
        // form of the data, see e.g. crbug.com/1243254.
        // Instead, any local changes will get committed the next time this
        // entity is changed.
      }
    }
  }

  static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
      ->TransferChangesTo(model_batch_updates->GetSyncMetadataChangeList());

  return {};
}

std::unique_ptr<syncer::DataBatch> ReadingListSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& url_string : storage_keys) {
    scoped_refptr<const ReadingListEntry> entry =
        model_->GetEntryByURL(GURL(url_string));
    if (entry) {
      AddEntryToBatch(batch.get(), *entry);
    }
  }

  return batch;
}

std::unique_ptr<syncer::DataBatch>
ReadingListSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const auto& url : model_->GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry =
        model_->GetEntryByURL(GURL(url));
    AddEntryToBatch(batch.get(), *entry);
  }

  return batch;
}

void ReadingListSyncBridge::AddEntryToBatch(syncer::MutableDataBatch* batch,
                                            const ReadingListEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<sync_pb::ReadingListSpecifics> entry_pb =
      entry.AsReadingListSpecifics();

  auto entity_data = std::make_unique<syncer::EntityData>();
  *(entity_data->specifics.mutable_reading_list()) = *entry_pb;
  entity_data->name = entry_pb->entry_id();

  batch->Put(entry_pb->entry_id(), std::move(entity_data));
}

std::string ReadingListSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetStorageKey(entity_data);
}

std::string ReadingListSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.reading_list().entry_id();
}

void ReadingListSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  base::AutoReset<bool> auto_reset_is_sync_stopping(
      &ongoing_apply_disable_sync_changes_, true);
  switch (wipe_model_upon_sync_disabled_behavior_) {
    case syncer::WipeModelUponSyncDisabledBehavior::kNever:
      CHECK_EQ(storage_type_for_uma_, syncer::StorageType::kUnspecified);
      // Fall back to the default behavior (delete metadata only).
      DataTypeSyncBridge::ApplyDisableSyncChanges(
          std::move(delete_metadata_change_list));
      break;
    case syncer::WipeModelUponSyncDisabledBehavior::kAlways:
      CHECK_EQ(storage_type_for_uma_, syncer::StorageType::kAccount);
      // For account storage, in addition to sync metadata deletion (which
      // |delete_metadata_change_list| represents), the actual reading list
      // entries need to be deleted. This function does both and is even
      // robust against orphan or unexpected data in storage.
      model_->SyncDeleteAllEntriesAndSyncMetadata();
      break;
    case syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata:
      CHECK_EQ(storage_type_for_uma_, syncer::StorageType::kUnspecified);
      syncer::SyncRecordModelClearedOnceHistogram(syncer::READING_LIST);
      wipe_model_upon_sync_disabled_behavior_ =
          syncer::WipeModelUponSyncDisabledBehavior::kNever;
      // `wipe_model_upon_sync_disabled_behavior_` being set to
      // `kOnceIfTrackingMetadata` implies metadata was being tracked when it
      // was loaded from storage, see logic in ModelReadyToSync().
      model_->SyncDeleteAllEntriesAndSyncMetadata();
      break;
  }
}

bool ReadingListSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  CHECK(entity_data.specifics.ByteSize() != 0);

  return ReadingListEntry::IsSpecificsValid(
      entity_data.specifics.reading_list());
}

bool ReadingListSyncBridge::CompareEntriesForSync(
    const sync_pb::ReadingListSpecifics& lhs,
    const sync_pb::ReadingListSpecifics& rhs) {
  DCHECK(lhs.entry_id() == rhs.entry_id());
  DCHECK(lhs.has_update_time_us());
  DCHECK(rhs.has_update_time_us());
  DCHECK(lhs.has_update_title_time_us());
  DCHECK(rhs.has_update_title_time_us());
  DCHECK(lhs.has_creation_time_us());
  DCHECK(rhs.has_creation_time_us());
  DCHECK(lhs.has_url());
  DCHECK(rhs.has_url());
  DCHECK(lhs.has_title());
  DCHECK(rhs.has_title());
  DCHECK(lhs.has_status());
  DCHECK(rhs.has_status());
  if (rhs.url() != lhs.url() ||
      rhs.update_title_time_us() < lhs.update_title_time_us() ||
      rhs.creation_time_us() < lhs.creation_time_us() ||
      rhs.update_time_us() < lhs.update_time_us()) {
    return false;
  }
  if (rhs.update_time_us() == lhs.update_time_us()) {
    if ((rhs.status() == sync_pb::ReadingListSpecifics::UNSEEN &&
         lhs.status() != sync_pb::ReadingListSpecifics::UNSEEN) ||
        (rhs.status() == sync_pb::ReadingListSpecifics::UNREAD &&
         lhs.status() == sync_pb::ReadingListSpecifics::READ))
      return false;
  }
  if (rhs.update_title_time_us() == lhs.update_title_time_us()) {
    if (rhs.title().compare(lhs.title()) < 0)
      return false;
  }
  if (rhs.creation_time_us() == lhs.creation_time_us()) {
    if (rhs.first_read_time_us() == 0 && lhs.first_read_time_us() != 0) {
      return false;
    }
    if (rhs.first_read_time_us() > lhs.first_read_time_us() &&
        lhs.first_read_time_us() != 0) {
      return false;
    }
  }
  return true;
}
