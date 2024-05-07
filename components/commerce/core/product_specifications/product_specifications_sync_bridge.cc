// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include <optional>
#include <set>

#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "components/sync/base/deletion_origin.h"
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
        OnSpecificsAdded(specifics);
        break;
      case syncer::EntityChange::ACTION_UPDATE: {
        auto local_specifics = entries_.find(change->storage_key());
        if (local_specifics != entries_.end()) {
          sync_pb::CompareSpecifics before = local_specifics->second;
          // Overwrite if specifics from sync are more recent.
          if (specifics.update_time_unix_epoch_micros() >
              local_specifics->second.update_time_unix_epoch_micros()) {
            entries_[change->storage_key()] = specifics;
            batch->WriteData(change->storage_key(),
                             specifics.SerializeAsString());
            OnSpecificsUpdated(before, specifics);
          }
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        auto it = entries_.find(change->storage_key());
        if (it == entries_.end()) {
          break;
        }
        ProductSpecificationsSet deleted_set =
            ProductSpecificationsSet::FromProto(it->second);
        entries_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        OnSpecificsRemoved(deleted_set);
        break;
    }
  }
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));
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

sync_pb::CompareSpecifics
ProductSpecificationsSyncBridge::AddProductSpecifications(
    const std::string& name,
    const std::vector<GURL>& urls) {
  // Sync is mandatory for this feature to be usable.
  CHECK(change_processor()->IsTrackingMetadata());

  sync_pb::CompareSpecifics specifics;
  specifics.set_uuid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  int64_t time_now = base::Time::Now().InMillisecondsSinceUnixEpoch();
  specifics.set_creation_time_unix_epoch_micros(time_now);
  specifics.set_update_time_unix_epoch_micros(time_now);
  specifics.set_name(name);
  for (const GURL& url : urls) {
    sync_pb::ComparisonData* comparison_data = specifics.add_data();
    comparison_data->set_url(url.spec());
  }

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  change_processor()->Put(specifics.uuid(), CreateEntityData(specifics),
                          batch->GetMetadataChangeList());

  entries_.emplace(specifics.uuid(), specifics);

  batch->WriteData(specifics.uuid(), specifics.SerializeAsString());
  Commit(std::move(batch));
  OnSpecificsAdded(specifics);
  return specifics;
}

sync_pb::CompareSpecifics
ProductSpecificationsSyncBridge::UpdateProductSpecificationsSet(
    const ProductSpecificationsSet& product_specs_set) {
  auto it = entries_.find(product_specs_set.uuid().AsLowercaseString());

  CHECK(it != entries_.end());

  // Sync is mandatory for this feature to be usable.
  CHECK(change_processor()->IsTrackingMetadata());

  sync_pb::CompareSpecifics before = it->second;
  sync_pb::CompareSpecifics& specifics = it->second;
  specifics.set_update_time_unix_epoch_micros(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  specifics.set_name(product_specs_set.name());

  specifics.clear_data();
  for (const GURL& url : product_specs_set.urls()) {
    sync_pb::ComparisonData* comparison_data = specifics.add_data();
    comparison_data->set_url(url.spec());
  }

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  change_processor()->Put(specifics.uuid(), CreateEntityData(specifics),
                          batch->GetMetadataChangeList());

  batch->WriteData(specifics.uuid(), specifics.SerializeAsString());
  Commit(std::move(batch));
  OnSpecificsUpdated(before, specifics);
  return specifics;
}

void ProductSpecificationsSyncBridge::DeleteProductSpecificationsSet(
    const std::string& uuid) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  auto it = entries_.find(uuid);
  if (it == entries_.end()) {
    return;
  }
  ProductSpecificationsSet deleted_set =
      ProductSpecificationsSet::FromProto(it->second);

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  change_processor()->Delete(uuid, syncer::DeletionOrigin::Unspecified(),
                             batch->GetMetadataChangeList());

  entries_.erase(uuid);
  batch->DeleteData(uuid);

  Commit(std::move(batch));
  OnSpecificsRemoved(deleted_set);
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

void ProductSpecificationsSyncBridge::Commit(
    std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ProductSpecificationsSyncBridge::OnCommit,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsSyncBridge::OnCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void ProductSpecificationsSyncBridge::AddObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
  observers_.AddObserver(observer);
}
void ProductSpecificationsSyncBridge::RemoveObserver(
    commerce::ProductSpecificationsSet::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProductSpecificationsSyncBridge::OnSpecificsAdded(
    const sync_pb::CompareSpecifics& compare_specifics) {
  for (auto& observer : observers_) {
    observer.OnProductSpecificationsSetAdded(
        ProductSpecificationsSet::FromProto(compare_specifics));
  }
}

void ProductSpecificationsSyncBridge::OnSpecificsUpdated(
    const sync_pb::CompareSpecifics& before,
    const sync_pb::CompareSpecifics& after) {
  for (auto& observer : observers_) {
    observer.OnProductSpecificationsSetUpdate(
        ProductSpecificationsSet::FromProto(before),
        ProductSpecificationsSet::FromProto(after));
  }
}

void ProductSpecificationsSyncBridge::OnSpecificsRemoved(
    const ProductSpecificationsSet& removed_set) {
  for (auto& observer : observers_) {
    observer.OnProductSpecificationsSetRemoved(removed_set);
  }
}

}  // namespace commerce
