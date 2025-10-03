// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_task_sync_bridge.h"

#include "base/functional/bind.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/contextual_task_specifics.pb.h"

namespace contextual_tasks {

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
  return std::nullopt;
}

std::optional<syncer::ModelError>
ContextualTaskSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> ContextualTaskSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
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
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {}

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
}

}  // namespace contextual_tasks
