// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_task_sync_bridge.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/contextual_tasks/internal/conversions.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/contextual_task_specifics.pb.h"

namespace contextual_tasks {

namespace {
sync_pb::AiThreadSpecifics::ThreadType ToProtoThreadType(
    ThreadType thread_type) {
  switch (thread_type) {
    case ThreadType::kUnknown:
      return sync_pb::AiThreadSpecifics::UNKNOWN;
    case ThreadType::kAiMode:
      return sync_pb::AiThreadSpecifics::AI_MODE;
  }
}

std::string StorageKeyFromUuid(const base::Uuid& uuid) {
  return uuid.AsLowercaseString();
}

// Create new EntityData object to contain specifics for writing changes.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromSpecifics(
    const sync_pb::ContextualTaskSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_contextual_task() = specifics;
  entity_data->name = specifics.guid();
  return entity_data;
}

// Extracts the task ID from a ContextualTaskEntity proto. A single contextual
// task can be represented by multiple entities (e.g., one for the main task
// and others for associated URL resources). This function ensures we can group
// all related entities by a common task ID.
std::string GetTaskIdFromContextualTaskEntity(
    const proto::ContextualTaskEntity& entity) {
  if (entity.specifics().has_contextual_task()) {
    return entity.specifics().guid();
  }

  if (entity.specifics().has_url_resource()) {
    return entity.specifics().url_resource().task_guid();
  }

  return std::string();
}

// Reconstructs a `ContextualTask` from its constituent entities. This
// involves processing a list of `ContextualTaskEntity` protos, each
// MUST be a part of the task (e.g., the main task details, an associated
// URL), and assembling them into a single, coherent `ContextualTask` object.
std::optional<ContextualTask> BuildTaskFromEntities(
    const std::string& task_id_str,
    const std::vector<proto::ContextualTaskEntity>& task_entities) {
  ContextualTask task(base::Uuid::ParseCaseInsensitive(task_id_str));
  bool has_contextual_task_specifics = false;
  for (const auto& entity : task_entities) {
    const auto& specifics = entity.specifics();
    if (specifics.has_contextual_task()) {
      const auto& task_proto = specifics.contextual_task();
      task.SetTitle(task_proto.title());
      if (task_proto.has_thread_id()) {
        task.AddThread(Thread(ToThreadType(task_proto.thread_type()),
                              task_proto.thread_id(), "", ""));
      }
      has_contextual_task_specifics = true;
    } else if (specifics.has_url_resource()) {
      const auto& url_resource_proto = specifics.url_resource();
      task.AddUrlResource(
          UrlResource(base::Uuid::ParseCaseInsensitive(specifics.guid()),
                      GURL(url_resource_proto.url())));
    }
  }
  if (has_contextual_task_specifics) {
    return task;
  } else {
    return std::nullopt;
  }
}

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
    contextual_task->set_thread_type(
        entity.specifics().contextual_task().thread_type());
  } else {
    sync_pb::UrlResource* url_resource =
        mutable_base_specifics->mutable_url_resource();
    url_resource->set_task_guid(entity.specifics().url_resource().task_guid());
    url_resource->set_url(entity.specifics().url_resource().url());
  }
}

proto::ContextualTaskEntity SpecificsToEntityProto(
    const sync_pb::ContextualTaskSpecifics& specifics) {
  proto::ContextualTaskEntity entity;
  entity.set_allocated_specifics(
      new sync_pb::ContextualTaskSpecifics(specifics));
  return entity;
}

proto::ContextualTaskEntity ContextualTaskToEntityProto(
    const ContextualTask& contextual_task) {
  CHECK(!contextual_task.IsEphemeral());
  proto::ContextualTaskEntity entity;
  sync_pb::ContextualTaskSpecifics* specifics = entity.mutable_specifics();
  specifics->set_guid(StorageKeyFromUuid(contextual_task.GetTaskId()));
  sync_pb::ContextualTask* task = specifics->mutable_contextual_task();
  task->set_title(contextual_task.GetTitle());
  if (contextual_task.GetThread()) {
    task->set_thread_id(contextual_task.GetThread()->server_id);
    task->set_thread_type(ToProtoThreadType(contextual_task.GetThread()->type));
  }
  return entity;
}

proto::ContextualTaskEntity UrlResourceToEntityProto(
    const base::Uuid& task_id,
    const UrlResource& url_resource) {
  proto::ContextualTaskEntity entity;
  sync_pb::ContextualTaskSpecifics* specifics = entity.mutable_specifics();
  specifics->set_guid(StorageKeyFromUuid(url_resource.url_id));
  sync_pb::UrlResource* resource = specifics->mutable_url_resource();
  resource->set_task_guid(StorageKeyFromUuid(task_id));
  resource->set_url(url_resource.url.spec());
  return entity;
}

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::ContextualTaskSpecifics& specific) {
  std::unique_ptr<syncer::EntityData> entity_data =
      std::make_unique<syncer::EntityData>();
  entity_data->name = specific.guid();
  entity_data->specifics.mutable_contextual_task()->CopyFrom(specific);
  return entity_data;
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
  std::vector<std::string> added_or_updated_guids;
  std::vector<base::Uuid> removed;
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const sync_pb::EntitySpecifics& entity_specifics = change->data().specifics;
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        CHECK(entity_specifics.has_contextual_task());
        const proto::ContextualTaskEntity& entity =
            SpecificsToEntityProto(entity_specifics.contextual_task());
        bool updated = false;
        if (change->type() == syncer::EntityChange::ACTION_ADD) {
          updated = AddEntityToMap(entity);
        } else {
          updated = UpdateEntityInMap(entity);
        }
        if (updated) {
          added_or_updated_guids.emplace_back(change->storage_key());
        }
        batch->WriteData(change->storage_key(), entity.SerializeAsString());
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        if (DeleteEntityFromMap(change->storage_key())) {
          removed.emplace_back(
              base::Uuid::ParseCaseInsensitive(change->storage_key()));
        }
        batch->DeleteData(change->storage_key());
        break;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  data_type_store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ContextualTaskSyncBridge::OnDataTypeStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));

  std::vector<ContextualTask> added_or_updated_task;
  for (const std::string& guid : added_or_updated_guids) {
    std::optional<ContextualTask> task = GetTaskById(guid);
    if (task) {
      added_or_updated_task.emplace_back(task.value());
    }
  }
  for (auto& observer : observers_) {
    observer.OnTaskAddedOrUpdatedRemotely(added_or_updated_task);
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
  for (const auto& [task_id, task_entities] : task_id_to_entities_map_) {
    for (const auto& entity : task_entities) {
      batch->Put(entity.specifics().guid(),
                 CreateEntityDataFromSpecifics(entity.specifics()));
    }
  }
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
  std::vector<base::Uuid> uuids;
  for (const auto& [task_id, entity] : task_id_to_entities_map_) {
    uuids.push_back(base::Uuid::ParseCaseInsensitive(task_id));
  }
  task_id_to_entities_map_.clear();
  data_type_store_->DeleteAllDataAndMetadata(base::DoNothing());
  weak_ptr_factory_.InvalidateWeakPtrs();
  for (auto& observer : observers_) {
    observer.OnTaskRemovedRemotely(uuids);
  }
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
  // LINT.IfChange(TrimAllSupportedFieldsFromRemoteSpecifics)
  sync_pb::ContextualTaskSpecifics trimmed_specifics =
      entity_specifics.contextual_task();
  trimmed_specifics.clear_guid();
  trimmed_specifics.clear_version();

  if (trimmed_specifics.has_contextual_task()) {
    sync_pb::ContextualTask* task = trimmed_specifics.mutable_contextual_task();
    task->clear_title();
    task->clear_thread_id();
    task->clear_thread_type();

    if (task->ByteSizeLong() == 0) {
      trimmed_specifics.clear_contextual_task();
    }
  }

  if (trimmed_specifics.has_url_resource()) {
    sync_pb::UrlResource* url_resource =
        trimmed_specifics.mutable_url_resource();
    url_resource->clear_task_guid();
    url_resource->clear_url();

    if (url_resource->ByteSizeLong() == 0) {
      trimmed_specifics.clear_url_resource();
    }
  }
  // LINT.ThenChange(//components/sync/protocol/contextual_task_specifics.proto:ContextualTaskSpecifics)

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  if (trimmed_specifics.ByteSizeLong() > 0) {
    *trimmed_entity_specifics.mutable_contextual_task() =
        std::move(trimmed_specifics);
  }
  return trimmed_entity_specifics;
}

std::vector<ContextualTask> ContextualTaskSyncBridge::GetTasks() const {
  std::vector<ContextualTask> tasks;
  for (auto& [task_id, task_entities] : task_id_to_entities_map_) {
    std::optional<ContextualTask> task =
        BuildTaskFromEntities(task_id, task_entities);
    if (task) {
      tasks.emplace_back(task.value());
    }
  }
  return tasks;
}

std::optional<ContextualTask> ContextualTaskSyncBridge::GetTaskById(
    const std::string& task_guid) const {
  auto it = task_id_to_entities_map_.find(task_guid);
  if (it != task_id_to_entities_map_.end()) {
    return BuildTaskFromEntities(task_guid, it->second);
  }
  return std::nullopt;
}

void ContextualTaskSyncBridge::OnTaskAddedLocally(
    const ContextualTask& contextual_task) {
  if (contextual_task.IsEphemeral()) {
    return;
  }

  proto::ContextualTaskEntity entity_proto =
      ContextualTaskToEntityProto(contextual_task);
  DCHECK(task_id_to_entities_map_.find(entity_proto.specifics().guid()) ==
         task_id_to_entities_map_.end());

  AddEntityToMap(entity_proto);
  UpsertEntityToSync(entity_proto);
}

void ContextualTaskSyncBridge::OnTaskRemovedLocally(const base::Uuid& task_id) {
  std::string task_id_str = StorageKeyFromUuid(task_id);
  auto it = task_id_to_entities_map_.find(task_id_str);

  if (it != task_id_to_entities_map_.end()) {
    // The vector contains the task entity itself plus all URL resources.
    std::vector<std::string> guids_to_remove;
    guids_to_remove.reserve(it->second.size());
    for (const auto& entity : it->second) {
      guids_to_remove.push_back(entity.specifics().guid());
    }

    task_id_to_entities_map_.erase(it);
    RemoveEntitiesFromSync(guids_to_remove);
  }
}

void ContextualTaskSyncBridge::OnTaskUpdatedLocally(
    const ContextualTask& contextual_task) {
  if (contextual_task.IsEphemeral()) {
    return;
  }

  proto::ContextualTaskEntity entity_proto =
      ContextualTaskToEntityProto(contextual_task);
  UpdateEntityInMap(entity_proto);
  UpsertEntityToSync(entity_proto);
}

void ContextualTaskSyncBridge::OnUrlAddedToTaskLocally(
    const base::Uuid& task_id,
    const UrlResource& url_resource) {
  proto::ContextualTaskEntity entity_proto =
      UrlResourceToEntityProto(task_id, url_resource);
  AddEntityToMap(entity_proto);
  UpsertEntityToSync(entity_proto);
}

void ContextualTaskSyncBridge::OnUrlRemovedFromTaskLocally(
    const base::Uuid& url_id) {
  std::string storage_key = StorageKeyFromUuid(url_id);
  DeleteEntityFromMap(storage_key);
  RemoveEntitiesFromSync({storage_key});
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

  for (const auto& record : *entries) {
    proto::ContextualTaskEntity entity;
    if (!entity.ParseFromString(record.value)) {
      change_processor()->ReportError(*error);
      return;
    }
    AddEntityToMap(entity);
  }

  for (auto& observer : observers_) {
    observer.OnContextualTaskDataStoreLoaded();
  }

  data_type_store_->ReadAllMetadata(
      base::BindOnce(&ContextualTaskSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool ContextualTaskSyncBridge::AddEntityToMap(
    const proto::ContextualTaskEntity& contextual_task_entity) {
  std::string task_id =
      GetTaskIdFromContextualTaskEntity(contextual_task_entity);
  if (task_id.empty()) {
    return false;
  }

  auto it = task_id_to_entities_map_.find(task_id);
  if (it != task_id_to_entities_map_.end()) {
    it->second.emplace_back(contextual_task_entity);
  } else {
    task_id_to_entities_map_.emplace(
        task_id,
        std::vector<proto::ContextualTaskEntity>{contextual_task_entity});
  }
  return true;
}

bool ContextualTaskSyncBridge::UpdateEntityInMap(
    const proto::ContextualTaskEntity& contextual_task_entity) {
  std::string task_id =
      GetTaskIdFromContextualTaskEntity(contextual_task_entity);

  if (task_id.empty()) {
    return false;
  }

  auto it = task_id_to_entities_map_.find(task_id);
  if (it == task_id_to_entities_map_.end()) {
    return false;
  }

  for (proto::ContextualTaskEntity& entity : it->second) {
    if (entity.specifics().guid() ==
        contextual_task_entity.specifics().guid()) {
      entity = contextual_task_entity;
      return true;
    }
  }
  return false;
}

bool ContextualTaskSyncBridge::DeleteEntityFromMap(const std::string& guid) {
  for (auto& [task_id, task_entities] : task_id_to_entities_map_) {
    for (auto it = task_entities.begin(); it != task_entities.end(); ++it) {
      if (it->specifics().guid() == guid) {
        task_entities.erase(it);
        return true;
      }
    }
  }
  return false;
}

std::optional<proto::ContextualTaskEntity>
ContextualTaskSyncBridge::GetEntityProto(const std::string& guid) {
  for (const auto& [task_id, task_entities] : task_id_to_entities_map_) {
    for (const auto& task : task_entities) {
      if (task.specifics().guid() == guid) {
        return task;
      }
    }
  }
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

void ContextualTaskSyncBridge::UpsertEntityToSync(
    const proto::ContextualTaskEntity& data) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      data_type_store_->CreateWriteBatch();
  batch->WriteData(data.specifics().guid(), data.SerializeAsString());
  if (change_processor()->IsTrackingMetadata()) {
    auto entity_data = CreateEntityData(data.specifics());
    // Copy because our key is the name of `entity_data`.
    std::string name = entity_data->name;
    change_processor()->Put(name, std::move(entity_data),
                            batch->GetMetadataChangeList());
  }
  data_type_store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ContextualTaskSyncBridge::OnDataTypeStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContextualTaskSyncBridge::RemoveEntitiesFromSync(
    const std::vector<std::string>& storage_keys) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      data_type_store_->CreateWriteBatch();
  for (const std::string& storage_key : storage_keys) {
    batch->DeleteData(storage_key);
    if (change_processor()->IsTrackingMetadata()) {
      change_processor()->Delete(storage_key,
                                 syncer::DeletionOrigin::Unspecified(),
                                 batch->GetMetadataChangeList());
    }
  }
  data_type_store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ContextualTaskSyncBridge::OnDataTypeStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace contextual_tasks
