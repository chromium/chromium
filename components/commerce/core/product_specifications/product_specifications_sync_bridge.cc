// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_sync_bridge.h"

#include <optional>
#include <set>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"
#include "url/gurl.h"

namespace {

syncer::EntityData MakeEntityData(
    const sync_pb::ProductComparisonSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_product_comparison() = specifics;
  entity_data.name = base::StringPrintf("%s_%s", specifics.name().c_str(),
                                        specifics.uuid().c_str());

  return entity_data;
}

bool IsMultiSpecSetsEnabled() {
  return base::FeatureList::IsEnabled(
      commerce::kProductSpecificationsMultiSpecifics);
}

}  // namespace

namespace commerce {

ProductSpecificationsSyncBridge::ProductSpecificationsSyncBridge(
    syncer::OnceDataTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    base::OnceCallback<void(void)> init_callback,
    Delegate* delegate)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      init_callback_(std::move(init_callback)),
      delegate_(delegate) {
  std::move(create_store_callback)
      .Run(syncer::PRODUCT_COMPARISON,
           base::BindOnce(&ProductSpecificationsSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

ProductSpecificationsSyncBridge::~ProductSpecificationsSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
ProductSpecificationsSyncBridge::CreateMetadataChangeList() {
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError>
ProductSpecificationsSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

std::optional<syncer::ModelError>
ProductSpecificationsSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  std::map<std::string, sync_pb::ProductComparisonSpecifics> prev_entries;
  if (IsMultiSpecSetsEnabled()) {
    for (const auto& [uuid, specific] : entries_) {
      prev_entries.emplace(uuid, specific);
    }
  }
  std::vector<sync_pb::ProductComparisonSpecifics> multi_specifics_changed;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    sync_pb::ProductComparisonSpecifics specifics =
        change->data().specifics.product_comparison();

    if (IsMultiSpecSetsEnabled()) {
      if (specifics.has_product_comparison() ||
          specifics.has_product_comparison_item()) {
        multi_specifics_changed.push_back(specifics);
      }

      // A delete only passes the Uuid, not the specifics itself which is
      // required for OnMultiSpecificsChanged to correctly detect deleted
      // ProductSpecificationsSets. Acquire specifics from local representation
      // so the specifics can be passed to OnMultiSpecificsChanged.
      // TODO(crbug.com/353982776) investigate OnMultiSpecificsChanged using
      // uuids instead to avoid special handling of this case.
      if (change->type() == syncer::EntityChange::ACTION_DELETE &&
          entries_.find(change->storage_key()) != entries_.end()) {
        multi_specifics_changed.push_back(
            entries_.find(change->storage_key())->second);
      }
    }
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
        entries_.emplace(change->storage_key(), specifics);
        batch->WriteData(change->storage_key(), specifics.SerializeAsString());
        if (!IsMultiSpecSetsEnabled()) {
          delegate_->OnSpecificsAdded({specifics});
        }
        break;
      case syncer::EntityChange::ACTION_UPDATE: {
        auto local_specifics = entries_.find(change->storage_key());
        if (local_specifics != entries_.end()) {
          sync_pb::ProductComparisonSpecifics before = local_specifics->second;
          // Overwrite if specifics from sync are more recent.
          if (specifics.update_time_unix_epoch_millis() >
              local_specifics->second.update_time_unix_epoch_millis()) {
            entries_[change->storage_key()] = specifics;
            batch->WriteData(change->storage_key(),
                             specifics.SerializeAsString());
            if (!IsMultiSpecSetsEnabled()) {
              delegate_->OnSpecificsUpdated({{before, specifics}});
            }
          }
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        auto it = entries_.find(change->storage_key());
        if (it == entries_.end()) {
          break;
        }
        sync_pb::ProductComparisonSpecifics deleted_specifics = it->second;

        entries_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        if (!IsMultiSpecSetsEnabled()) {
          delegate_->OnSpecificsRemoved({deleted_specifics});
        }
        break;
    }
  }
  if (IsMultiSpecSetsEnabled()) {
    delegate_->OnMultiSpecificsChanged(multi_specifics_changed, prev_entries);
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));
  return {};
}

std::string ProductSpecificationsSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.product_comparison().uuid();
}

std::string ProductSpecificationsSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::unique_ptr<syncer::DataBatch>
ProductSpecificationsSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    if (auto it = entries_.find(storage_key); it != entries_.end()) {
      batch->Put(storage_key, CreateEntityData(it->second));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
ProductSpecificationsSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (auto& entry : entries_) {
    batch->Put(entry.first, CreateEntityData(entry.second));
  }
  return batch;
}

sync_pb::EntitySpecifics
ProductSpecificationsSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  const sync_pb::ProductComparisonSpecifics trimmed_specifics =
      TrimSpecificsForCaching(entity_specifics.product_comparison());

  if (trimmed_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_product_comparison() = trimmed_specifics;
  return trimmed_entity_specifics;
}

bool ProductSpecificationsSyncBridge::IsSyncEnabled() {
  return change_processor()->IsTrackingMetadata();
}

void ProductSpecificationsSyncBridge::AddSpecifics(
    const std::vector<sync_pb::ProductComparisonSpecifics> specifics) {
  // Sync is mandatory for this feature to be usable.
  CHECK(change_processor()->IsTrackingMetadata());

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const auto& specific : specifics) {
    change_processor()->Put(specific.uuid(), CreateEntityData(specific),
                            batch->GetMetadataChangeList());
    batch->WriteData(specific.uuid(), specific.SerializeAsString());
    entries_.emplace(specific.uuid(), specific);
  }

  Commit(std::move(batch));
}

void ProductSpecificationsSyncBridge::UpdateSpecifics(
    const sync_pb::ProductComparisonSpecifics& new_specifics) {
  CHECK(entries_.find(new_specifics.uuid()) != entries_.end());

  // Sync is mandatory for this feature to be usable.
  CHECK(change_processor()->IsTrackingMetadata());

  entries_[new_specifics.uuid()] = new_specifics;

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  change_processor()->Put(new_specifics.uuid(), CreateEntityData(new_specifics),
                          batch->GetMetadataChangeList());

  batch->WriteData(new_specifics.uuid(), new_specifics.SerializeAsString());
  Commit(std::move(batch));
}

void ProductSpecificationsSyncBridge::DeleteSpecifics(
    const std::vector<sync_pb::ProductComparisonSpecifics> to_remove) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (auto& specifics : to_remove) {
    change_processor()->Delete(specifics.uuid(),
                               syncer::DeletionOrigin::Unspecified(),
                               batch->GetMetadataChangeList());

    entries_.erase(specifics.uuid());
    batch->DeleteData(specifics.uuid());
  }

  Commit(std::move(batch));
}

void ProductSpecificationsSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&ProductSpecificationsSyncBridge::OnReadAllDataAndMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProductSpecificationsSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> record_list,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  // If Metadata cache contains supported fields it is because the browser
  // has been upgraded. metadata_batch and record_list are no longer usable.
  if (base::FeatureList::IsEnabled(
          kProductSpecificationsClearMetadataOnNewlySupportedFields) &&
      SyncMetadataCacheContainsSupportedFields(
          metadata_batch->GetAllMetadata())) {
    store_->DeleteAllDataAndMetadata(base::DoNothing());
    metadata_batch = std::make_unique<syncer::MetadataBatch>();
    record_list = std::make_unique<syncer::DataTypeStore::RecordList>();
  }

  for (const syncer::DataTypeStore::Record& record : *record_list) {
    sync_pb::ProductComparisonSpecifics product_comparison_specifics;
    if (!product_comparison_specifics.ParseFromString(record.value)) {
      continue;
    }
    entries_.emplace(product_comparison_specifics.uuid(),
                     product_comparison_specifics);
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  DCHECK(init_callback_);
  if (init_callback_) {
    std::move(init_callback_).Run();
  }
}

void ProductSpecificationsSyncBridge::Commit(
    std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ProductSpecificationsSyncBridge::OnCommit,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool ProductSpecificationsSyncBridge::SyncMetadataCacheContainsSupportedFields(
    const syncer::EntityMetadataMap& metadata_map) const {
  for (const auto& metadata_entry : metadata_map) {
    // Serialize the cached specifics and parse them back to a proto. Fields
    // which were unsupported that have become supported should be parsed
    // correctly.
    std::string serialized_specifics;
    metadata_entry.second->possibly_trimmed_base_specifics().SerializeToString(
        &serialized_specifics);
    sync_pb::EntitySpecifics parsed_specifics;
    parsed_specifics.ParseFromString(serialized_specifics);

    // If `parsed_specifics` contain any supported fields, they would be cleared
    // by the trimming function.
    if (parsed_specifics.ByteSizeLong() !=
        TrimAllSupportedFieldsFromRemoteSpecifics(parsed_specifics)
            .ByteSizeLong()) {
      return true;
    }
  }

  return false;
}

void ProductSpecificationsSyncBridge::OnCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

const sync_pb::ProductComparisonSpecifics&
ProductSpecificationsSyncBridge::GetPossiblyTrimmedPasswordSpecificsData(
    const std::string& storage_key) {
  return change_processor()
      ->GetPossiblyTrimmedRemoteSpecifics(storage_key)
      .product_comparison();
}

std::unique_ptr<syncer::EntityData>
ProductSpecificationsSyncBridge::CreateEntityData(
    const sync_pb::ProductComparisonSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  sync_pb::ProductComparisonSpecifics* entity_specifics =
      entity_data->specifics.mutable_product_comparison();

  *entity_specifics = GetPossiblyTrimmedPasswordSpecificsData(specifics.uuid());

  entity_specifics->set_uuid(specifics.uuid());
  entity_specifics->set_creation_time_unix_epoch_millis(
      specifics.creation_time_unix_epoch_millis());
  entity_specifics->set_update_time_unix_epoch_millis(
      specifics.update_time_unix_epoch_millis());
  entity_specifics->set_name(specifics.name());

  for (const sync_pb::ComparisonData& data_to_copy : specifics.data()) {
    sync_pb::ComparisonData* data = entity_specifics->add_data();
    data->set_url(data_to_copy.url());
  }

  if (IsMultiSpecSetsEnabled() && specifics.has_product_comparison()) {
    *entity_specifics->mutable_product_comparison() =
        specifics.product_comparison();
    entity_data->name =
        base::StringPrintf("product_comparison_%s_%s", specifics.uuid().c_str(),
                           specifics.product_comparison().name().c_str());
  } else if (IsMultiSpecSetsEnabled() &&
             specifics.has_product_comparison_item()) {
    *entity_specifics->mutable_product_comparison_item() =
        specifics.product_comparison_item();
    entity_data->name = base::StringPrintf(
        "product_comparison_item_%s_%s",
        specifics.product_comparison_item().product_comparison_uuid().c_str(),
        specifics.uuid().c_str());
  } else if (specifics.has_name()) {
    entity_data->name = base::StringPrintf("%s_%s", specifics.name().c_str(),
                                           specifics.uuid().c_str());
  } else {
    // TODO(crbug.com/354017278) remove this when the multi specifics
    // flag is removed.
    entity_data->name = base::StringPrintf("%s", specifics.uuid().c_str());
  }
  return entity_data;
}

const sync_pb::ProductComparisonSpecifics
ProductSpecificationsSyncBridge::TrimSpecificsForCaching(
    const sync_pb::ProductComparisonSpecifics& comparison_specifics) const {
  sync_pb::ProductComparisonSpecifics trimmed_comparison_data =
      sync_pb::ProductComparisonSpecifics(comparison_specifics);
  trimmed_comparison_data.clear_uuid();
  trimmed_comparison_data.clear_creation_time_unix_epoch_millis();
  trimmed_comparison_data.clear_update_time_unix_epoch_millis();
  trimmed_comparison_data.clear_name();
  trimmed_comparison_data.clear_data();
  if (IsMultiSpecSetsEnabled()) {
    trimmed_comparison_data.clear_product_comparison();
    trimmed_comparison_data.clear_product_comparison_item();
  }
  return trimmed_comparison_data;
}

void ProductSpecificationsSyncBridge::ApplyIncrementalSyncChangesForTesting(
    const std::vector<std::pair<sync_pb::ProductComparisonSpecifics,
                                syncer::EntityChange::ChangeType>>&
        specifics_to_change) {
  syncer::EntityChangeList changes;
  for (const auto& [specifics, change_type] : specifics_to_change) {
    switch (change_type) {
      case syncer::EntityChange::ACTION_ADD:
        changes.push_back(syncer::EntityChange::CreateAdd(
            specifics.uuid(), MakeEntityData(specifics)));
        break;
      case syncer::EntityChange::ACTION_UPDATE:
        changes.push_back(syncer::EntityChange::CreateUpdate(
            specifics.uuid(), MakeEntityData(specifics)));
        break;
      case syncer::EntityChange::ACTION_DELETE:
        changes.push_back(syncer::EntityChange::CreateDelete(specifics.uuid()));
        break;
      default:
        DCHECK(0) << "EntityChange " << change_type << "not supported\n";
    }
  }

  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                              std::move(changes));
}

}  // namespace commerce
