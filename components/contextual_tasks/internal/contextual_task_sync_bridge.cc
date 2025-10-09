// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_task_sync_bridge.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/contextual_task_specifics.pb.h"

namespace contextual_tasks {

namespace {

void ApplyEntityProtoToTrimmedSpecifics(
    const proto::ContextualTaskEntity& entity,
    sync_pb::ContextualTaskSpecifics* mutable_base_specifics) {
  mutable_base_specifics->set_guid(entity.specifics().guid());
  if (entity.specifics().has_contextual_task()) {
    sync_pb::ContextualTask* contextual_task =
        mutable_base_specifics->mutable_contextual_task();
    contextual_task->set_title(entity.specifics().contextual_task().title());
    contextual_task->set_thread_id(
        entity.specifics().contextual_task().thread_id());
  } else {
    sync_pb::UrlResource* url_resource =
        mutable_base_specifics->mutable_url_resource();
    url_resource->set_task_guid(entity.specifics().url_resource().task_guid());
    url_resource->set_url(entity.specifics().url_resource().url());
  }
}

proto::ContextualTaskEntity SpecificsToEntityProto(
    const sync_pb::ContextualTaskSpecifics specifics) {
  proto::ContextualTaskEntity entity;
  entity.set_allocated_specifics(
      new sync_pb::ContextualTaskSpecifics(specifics));
  return entity;
}

}  // namespace

ContextualTaskSyncBridge::ContextualTaskSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::CONTEXTUAL_TASK,
           base::BindOnce(&ContextualTaskSyncBridge::OnDataTypeStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

ContextualTaskSyncBridge::~ContextualTaskSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
ContextualTaskSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> ContextualTaskSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_change_list));
}

std::optional<syncer::ModelError>
ContextualTaskSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      data_type_store_->CreateWriteBatch();
  std::vector<proto::ContextualTaskEntity> added_or_updated;
  std::vector<base::Uuid> removed;
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const sync_pb::EntitySpecifics& entity_specifics = change->data().specifics;
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        CHECK(entity_specifics.has_contextual_task());
        proto::ContextualTaskEntity entity =
            SpecificsToEntityProto(entity_specifics.contextual_task());
        added_or_updated.emplace_back(entity);
        batch->WriteData(change->storage_key(), entity.SerializeAsString());
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        removed.emplace_back(
            base::Uuid::ParseCaseInsensitive(change->storage_key()));
        batch->DeleteData(change->storage_key());
        break;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  data_type_store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ContextualTaskSyncBridge::OnDataTypeStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
  for (auto& observer : observers_) {
    observer.OnTaskAddedOrUpdatedRemotely(added_or_updated);
    observer.OnTaskRemovedRemotely(removed);
  }

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> ContextualTaskSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& key : storage_keys) {
    std::optional<proto::ContextualTaskEntity> entity = GetEntityProto(key);
    if (entity) {
      auto entity_data = std::make_unique<syncer::EntityData>();
      entity_data->specifics =
          change_processor()->GetPossiblyTrimmedRemoteSpecifics(key);
      ApplyEntityProtoToTrimmedSpecifics(
          entity.value(), entity_data->specifics.mutable_contextual_task());
      batch->Put(key, std::move(entity_data));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
ContextualTaskSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  return batch;
}

std::string ContextualTaskSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string ContextualTaskSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.contextual_task().guid();
}

void ContextualTaskSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  data_type_store_->DeleteAllDataAndMetadata(base::DoNothing());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool ContextualTaskSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  const sync_pb::ContextualTaskSpecifics& specifics =
      entity_data.specifics.contextual_task();
  return specifics.has_contextual_task() || specifics.has_url_resource();
}

sync_pb::EntitySpecifics
ContextualTaskSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  sync_pb::ContextualTaskSpecifics trimmed_specifics =
      entity_specifics.contextual_task();
  trimmed_specifics.clear_guid();
  trimmed_specifics.clear_version();

  if (trimmed_specifics.has_contextual_task()) {
    sync_pb::ContextualTask* task = trimmed_specifics.mutable_contextual_task();
    task->clear_title();
    task->clear_thread_id();
  }

  if (trimmed_specifics.has_url_resource()) {
    sync_pb::UrlResource* url_resource =
        trimmed_specifics.mutable_url_resource();
    url_resource->clear_task_guid();
    url_resource->clear_url();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_contextual_task() =
      std::move(trimmed_specifics);
  return trimmed_entity_specifics;
}

void ContextualTaskSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTaskSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContextualTaskSyncBridge::OnDataTypeStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  data_type_store_ = std::move(store);

  data_type_store_->ReadAllData(
      base::BindOnce(&ContextualTaskSyncBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContextualTaskSyncBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  std::vector<proto::ContextualTaskEntity> loaded_entities;
  for (const auto& record : *entries) {
    proto::ContextualTaskEntity entity;
    if (!entity.ParseFromString(record.value)) {
      change_processor()->ReportError(*error);
      return;
    }
    loaded_entities.emplace_back(std::move(entity));
  }

  for (auto& observer : observers_) {
    observer.OnContextualTaskDataStoreLoaded(loaded_entities);
  }

  data_type_store_->ReadAllMetadata(
      base::BindOnce(&ContextualTaskSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::optional<proto::ContextualTaskEntity>
ContextualTaskSyncBridge::GetEntityProto(const std::string& guid) {
  return std::nullopt;
}

void ContextualTaskSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void ContextualTaskSyncBridge::OnDataTypeStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace contextual_tasks
