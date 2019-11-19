// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_store.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "components/reading_list/core/proto/reading_list.pb.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"

ReadingListStore::ReadingListStore(
    syncer::OnceModelTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ReadingListModelStorage(std::move(change_processor)),
      create_store_callback_(std::move(create_store_callback)),
      pending_transaction_count_(0) {}

ReadingListStore::~ReadingListStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(0, pending_transaction_count_);
}

void ReadingListStore::SetReadingListModel(ReadingListModel* model,
                                           ReadingListStoreDelegate* delegate,
                                           base::Clock* clock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  model_ = model;
  delegate_ = delegate;
  clock_ = clock;
  std::move(create_store_callback_)
      .Run(syncer::READING_LIST,
           base::BindOnce(&ReadingListStore::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<ReadingListModelStorage::ScopedBatchUpdate>
ReadingListStore::EnsureBatchCreated() {
  return std::make_unique<ScopedBatchUpdate>(this);
}

ReadingListStore::ScopedBatchUpdate::ScopedBatchUpdate(ReadingListStore* store)
    : store_(store) {
  store_->BeginTransaction();
}

ReadingListStore::ScopedBatchUpdate::~ScopedBatchUpdate() {
  store_->CommitTransaction();
}

void ReadingListStore::BeginTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_transaction_count_++;
  if (pending_transaction_count_ == 1) {
    batch_ = store_->CreateWriteBatch();
  }
}

void ReadingListStore::CommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_transaction_count_--;
  if (pending_transaction_count_ == 0) {
    store_->CommitWriteBatch(std::move(batch_),
                             base::BindOnce(&ReadingListStore::OnDatabaseSave,
                                            weak_ptr_factory_.GetWeakPtr()));
    batch_.reset();
  }
}

void ReadingListStore::SaveEntry(const ReadingListEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto token = EnsureBatchCreated();

  std::unique_ptr<reading_list::ReadingListLocal> pb_entry =
      entry.AsReadingListLocal(clock_->Now());

  batch_->WriteData(entry.URL().spec(), pb_entry->SerializeAsString());

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }
  std::unique_ptr<sync_pb::ReadingListSpecifics> pb_entry_sync =
      entry.AsReadingListSpecifics();

  std::unique_ptr<syncer::EntityData> entity_data(new syncer::EntityData());
  *entity_data->specifics.mutable_reading_list() = *pb_entry_sync;
  entity_data->name = pb_entry_sync->entry_id();

  change_processor()->Put(entry.URL().spec(), std::move(entity_data),
                          batch_->GetMetadataChangeList());
}

void ReadingListStore::RemoveEntry(const ReadingListEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto token = EnsureBatchCreated();

  batch_->DeleteData(entry.URL().spec());
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }
  change_processor()->Delete(entry.URL().spec(),
                             batch_->GetMetadataChangeList());
}

void ReadingListStore::OnDatabaseLoad(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  auto loaded_entries =
      std::make_unique<ReadingListStoreDelegate::ReadingListEntries>();

  for (const syncer::ModelTypeStore::Record& r : *entries) {
    reading_list::ReadingListLocal proto;
    if (!proto.ParseFromString(r.value)) {
      continue;
      // TODO(skym, crbug.com/582460): Handle unrecoverable initialization
      // failure.
    }

    std::unique_ptr<ReadingListEntry> entry(
        ReadingListEntry::FromReadingListLocal(proto, clock_->Now()));
    if (!entry) {
      continue;
    }
    GURL url = entry->URL();
    DCHECK(!loaded_entries->count(url));
    loaded_entries->insert(std::make_pair(url, std::move(*entry)));
  }

  delegate_->StoreLoaded(std::move(loaded_entries));

  store_->ReadAllMetadata(base::BindOnce(&ReadingListStore::OnReadAllMetadata,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void ReadingListStore::OnReadAllMetadata(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to read metadata."});
  } else {
    change_processor()->ModelReadyToSync(std::move(metadata_batch));
  }
}

void ReadingListStore::OnDatabaseSave(
    const base::Optional<syncer::ModelError>& error) {
  return;
}

void ReadingListStore::OnStoreCreated(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    // TODO(crbug.com/664926): handle store creation error.
    return;
  }
  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&ReadingListStore::OnDatabaseLoad,
                                     weak_ptr_factory_.GetWeakPtr()));
  return;
}

// Creates an object used to communicate changes in the sync metadata to the
// model type store.
std::unique_ptr<syncer::MetadataChangeList>
ReadingListStore::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
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
base::Optional<syncer::ModelError> ReadingListStore::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto token = EnsureBatchCreated();
  // Keep track of the last update of each item.
  std::set<std::string> synced_entries;
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
      model_batch_updates = model_->BeginBatchUpdates();

  // Merge sync to local data.
  for (const auto& change : entity_data) {
    synced_entries.insert(change->storage_key());
    const sync_pb::ReadingListSpecifics& specifics =
        change->data().specifics.reading_list();
    // Deserialize entry.
    std::unique_ptr<ReadingListEntry> entry(
        ReadingListEntry::FromReadingListSpecifics(specifics, clock_->Now()));

    const ReadingListEntry* existing_entry =
        model_->GetEntryByURL(entry->URL());

    if (!existing_entry) {
      // This entry is new. Add it to the store and model.
      // Convert to local store format and write to store.
      std::unique_ptr<reading_list::ReadingListLocal> entry_pb =
          entry->AsReadingListLocal(clock_->Now());
      batch_->WriteData(entry->URL().spec(), entry_pb->SerializeAsString());

      // Notify model about updated entry.
      delegate_->SyncAddEntry(std::move(entry));
    } else {
      // Merge the local data and the sync data and store the result.
      ReadingListEntry* merged_entry =
          delegate_->SyncMergeEntry(std::move(entry));

      // Write to the store.
      std::unique_ptr<reading_list::ReadingListLocal> entry_local_pb =
          merged_entry->AsReadingListLocal(clock_->Now());
      batch_->WriteData(merged_entry->URL().spec(),
                        entry_local_pb->SerializeAsString());

      // Send to sync
      std::unique_ptr<sync_pb::ReadingListSpecifics> entry_sync_pb =
          merged_entry->AsReadingListSpecifics();
      DCHECK(CompareEntriesForSync(specifics, *entry_sync_pb));
      auto entity_data = std::make_unique<syncer::EntityData>();
      *(entity_data->specifics.mutable_reading_list()) = *entry_sync_pb;
      entity_data->name = entry_sync_pb->entry_id();

      // TODO(crbug.com/666232): Investigate if there is a risk of sync
      // ping-pong.
      change_processor()->Put(entry_sync_pb->entry_id(), std::move(entity_data),
                              metadata_change_list.get());
    }
  }

  // Commit local only entries to server.
  for (const auto& url : model_->Keys()) {
    const ReadingListEntry* entry = model_->GetEntryByURL(url);
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
  batch_->TakeMetadataChangesFrom(std::move(metadata_change_list));

  return {};
}

// Apply changes from the sync server locally.
// Please note that |entity_changes| might have fewer entries than
// |metadata_change_list| in case when some of the data changes are filtered
// out, or even be empty in case when a commit confirmation is processed and
// only the metadata needs to persisted.
base::Optional<syncer::ModelError> ReadingListStore::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate> batch =
      model_->BeginBatchUpdates();
  auto token = EnsureBatchCreated();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      batch_->DeleteData(change->storage_key());
      // Need to notify model that entry is deleted.
      delegate_->SyncRemoveEntry(GURL(change->storage_key()));
    } else {
      // Deserialize entry.
      const sync_pb::ReadingListSpecifics& specifics =
          change->data().specifics.reading_list();
      std::unique_ptr<ReadingListEntry> entry(
          ReadingListEntry::FromReadingListSpecifics(specifics, clock_->Now()));

      const ReadingListEntry* existing_entry =
          model_->GetEntryByURL(entry->URL());

      if (!existing_entry) {
        // This entry is new. Add it to the store and model.
        // Convert to local store format and write to store.
        std::unique_ptr<reading_list::ReadingListLocal> entry_pb =
            entry->AsReadingListLocal(clock_->Now());
        batch_->WriteData(entry->URL().spec(), entry_pb->SerializeAsString());

        // Notify model about updated entry.
        delegate_->SyncAddEntry(std::move(entry));
      } else {
        // Merge the local data and the sync data and store the result.
        ReadingListEntry* merged_entry =
            delegate_->SyncMergeEntry(std::move(entry));

        // Write to the store.
        std::unique_ptr<reading_list::ReadingListLocal> entry_local_pb =
            merged_entry->AsReadingListLocal(clock_->Now());
        batch_->WriteData(merged_entry->URL().spec(),
                          entry_local_pb->SerializeAsString());

        // Send to sync
        std::unique_ptr<sync_pb::ReadingListSpecifics> entry_sync_pb =
            merged_entry->AsReadingListSpecifics();
        DCHECK(CompareEntriesForSync(specifics, *entry_sync_pb));
        auto entity_data = std::make_unique<syncer::EntityData>();
        *(entity_data->specifics.mutable_reading_list()) = *entry_sync_pb;
        entity_data->name = entry_sync_pb->entry_id();

        // TODO(crbug.com/666232): Investigate if there is a risk of sync
        // ping-pong.
        change_processor()->Put(entry_sync_pb->entry_id(),
                                std::move(entity_data),
                                metadata_change_list.get());
      }
    }
  }

  batch_->TakeMetadataChangesFrom(std::move(metadata_change_list));
  return {};
}

void ReadingListStore::GetData(StorageKeyList storage_keys,
                               DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& url_string : storage_keys) {
    const ReadingListEntry* entry = model_->GetEntryByURL(GURL(url_string));
    if (entry) {
      AddEntryToBatch(batch.get(), *entry);
    }
  }

  std::move(callback).Run(std::move(batch));
}

void ReadingListStore::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const auto& url : model_->Keys()) {
    const ReadingListEntry* entry = model_->GetEntryByURL(GURL(url));
    AddEntryToBatch(batch.get(), *entry);
  }

  std::move(callback).Run(std::move(batch));
}

void ReadingListStore::AddEntryToBatch(syncer::MutableDataBatch* batch,
                                       const ReadingListEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<sync_pb::ReadingListSpecifics> entry_pb =
      entry.AsReadingListSpecifics();

  std::unique_ptr<syncer::EntityData> entity_data(new syncer::EntityData());
  *(entity_data->specifics.mutable_reading_list()) = *entry_pb;
  entity_data->name = entry_pb->entry_id();

  batch->Put(entry_pb->entry_id(), std::move(entity_data));
}

// Get or generate a client tag for |entity_data|. This must be the same tag
// that was/would have been generated in the SyncableService/Directory world
// for backward compatibility with pre-USS clients. The only time this
// theoretically needs to be called is on the creation of local data, however
// it is also used to verify the hash of remote data. If a data type was never
// launched pre-USS, then method does not need to be different from
// GetStorageKey().
std::string ReadingListStore::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetStorageKey(entity_data);
}

// Get or generate a storage key for |entity_data|. This will only ever be
// called once when first encountering a remote entity. Local changes will
// provide their storage keys directly to Put instead of using this method.
// Theoretically this function doesn't need to be stable across multiple calls
// on the same or different clients, but to keep things simple, it probably
// should be.
std::string ReadingListStore::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.reading_list().entry_id();
}

bool ReadingListStore::CompareEntriesForSync(
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
