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
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"

ReadingListSyncBridge::ReadingListSyncBridge(
    syncer::StorageType storage_type,
    base::Clock* clock,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)),
      storage_type_(storage_type),
      clock_(clock) {}

ReadingListSyncBridge::~ReadingListSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ReadingListSyncBridge::ModelReadyToSync(
    ReadingListModelImpl* model,
    std::unique_ptr<syncer::MetadataBatch> sync_metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model);
  DCHECK(!model_);

  model_ = model;
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
    syncer::MetadataChangeList* metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  change_processor()->Delete(entry.URL().spec(), metadata_change_list);
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
// Delete(...) can also be called but should not be needed for most model types.
// Durable storage writes, if not able to combine all change atomically, should
// save the metadata after the data changes, so that this merge will be re-
// driven by sync if is not completely saved during the current run.
absl::optional<syncer::ModelError> ReadingListSyncBridge::MergeSyncData(
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
    // Deserialize entry.
    scoped_refptr<ReadingListEntry> entry(
        ReadingListEntry::FromReadingListSpecifics(specifics, clock_->Now()));

    scoped_refptr<const ReadingListEntry> existing_entry =
        model_->GetEntryByURL(entry->URL());

    if (!existing_entry) {
      model_->SyncAddEntry(std::move(entry));
    } else {
      ReadingListEntry* merged_entry = model_->SyncMergeEntry(std::move(entry));

      // Send to sync
      std::unique_ptr<sync_pb::ReadingListSpecifics> entry_sync_pb =
          merged_entry->AsReadingListSpecifics();
#if !defined(NDEBUG)
      scoped_refptr<ReadingListEntry> initial_entry(
          ReadingListEntry::FromReadingListSpecifics(specifics, clock_->Now()));
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
absl::optional<syncer::ModelError> ReadingListSyncBridge::ApplySyncChanges(
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
      scoped_refptr<ReadingListEntry> entry(
          ReadingListEntry::FromReadingListSpecifics(specifics, clock_->Now()));

      scoped_refptr<const ReadingListEntry> existing_entry =
          model_->GetEntryByURL(entry->URL());

      if (!existing_entry) {
        model_->SyncAddEntry(std::move(entry));
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

void ReadingListSyncBridge::GetData(StorageKeyList storage_keys,
                                    DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& url_string : storage_keys) {
    scoped_refptr<const ReadingListEntry> entry =
        model_->GetEntryByURL(GURL(url_string));
    if (entry) {
      AddEntryToBatch(batch.get(), *entry);
    }
  }

  std::move(callback).Run(std::move(batch));
}

void ReadingListSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const auto& url : model_->GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry =
        model_->GetEntryByURL(GURL(url));
    AddEntryToBatch(batch.get(), *entry);
  }

  std::move(callback).Run(std::move(batch));
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

void ReadingListSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  // A non-null |delete_metadata_change_list| indicates sync (or reading list
  // sync) is permanently disabled (as opposed to temporarily paused).
  if (delete_metadata_change_list) {
    switch (storage_type_) {
      case syncer::StorageType::kUnspecified:
        // Fall back to the default behavior.
        break;
      case syncer::StorageType::kAccount:
        // For account storage, in addition to sync metadata deletion (which
        // |delete_metadata_change_list| represents), the actual reading list
        // entries need to be deleted. This function does both and is even
        // robust against orphan or unexpected data in storage.
        model_->SyncDeleteAllEntriesAndSyncMetadata();
        break;
    }
  }

  // Exercise the default codepath to be safe (but should be a no-op).
  ModelTypeSyncBridge::ApplyStopSyncChanges(
      std::move(delete_metadata_change_list));
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
