// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include <set>

#include "base/strings/stringprintf.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/compare_specifics.pb.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::CompareSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = base::StringPrintf("%s_%s", specifics.name().c_str(),
                                         specifics.uuid().c_str());
  entity_data->specifics.mutable_compare()->CopyFrom(specifics);
  return entity_data;
}

}  // namespace

namespace commerce {

ProductSpecificationsSyncBridge::ProductSpecificationsSyncBridge(
    syncer::OnceModelTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  std::move(create_store_callback)
      .Run(syncer::COMPARE,
           base::BindOnce(&ProductSpecificationsSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

ProductSpecificationsSyncBridge::~ProductSpecificationsSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
ProductSpecificationsSyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError>
ProductSpecificationsSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK(entries_.empty());
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}
std::optional<syncer::ModelError>
ProductSpecificationsSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const sync_pb::CompareSpecifics& specifics =
        change->data().specifics.compare();
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
        entries_.emplace(change->storage_key(), specifics);
        batch->WriteData(change->storage_key(), specifics.SerializeAsString());
        break;
      case syncer::EntityChange::ACTION_UPDATE: {
        auto local_specifics = entries_.find(change->storage_key());
        if (local_specifics != entries_.end()) {
          // Overwrite if specifics from sync are more recent.
          if (specifics.update_time_unix_epoch_micros() >
              local_specifics->second.update_time_unix_epoch_micros()) {
            entries_[change->storage_key()] = specifics;
            batch->WriteData(change->storage_key(),
                             specifics.SerializeAsString());
          }
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        entries_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        break;
    }
  }
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ProductSpecificationsSyncBridge::OnCommit,
                     weak_ptr_factory_.GetWeakPtr()));
  return {};
}

std::string ProductSpecificationsSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.compare().uuid();
}

std::string ProductSpecificationsSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

void ProductSpecificationsSyncBridge::GetData(StorageKeyList storage_keys,
                                              DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    if (auto it = entries_.find(storage_key); it != entries_.end()) {
      batch->Put(storage_key, CreateEntityData(it->second));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void ProductSpecificationsSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (auto& entry : entries_) {
    batch->Put(entry.first, CreateEntityData(entry.second));
  }
  std::move(callback).Run(std::move(batch));
}

void ProductSpecificationsSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(
      base::BindOnce(&ProductSpecificationsSyncBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsSyncBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  store_->ReadAllMetadata(
      base::BindOnce(&ProductSpecificationsSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(record_list)));
}

void ProductSpecificationsSyncBridge::OnReadAllMetadata(
    std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError({FROM_HERE, "Failed to read metadata."});
    return;
  }

  for (const syncer::ModelTypeStore::Record& record : *record_list) {
    sync_pb::CompareSpecifics compare_specifics;
    if (!compare_specifics.ParseFromString(record.value)) {
      continue;
    }
    entries_.emplace(compare_specifics.uuid(), compare_specifics);
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void ProductSpecificationsSyncBridge::OnCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace commerce
