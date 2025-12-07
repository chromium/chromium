// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/ai_thread_sync_bridge.h"

#include "base/functional/callback_helpers.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"

namespace contextual_tasks {

namespace {

// Create new EntityData object to contain specifics for writing changes.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromSpecifics(
    const sync_pb::AiThreadSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_ai_thread() = specifics;
  entity_data->name = specifics.server_id();
  return entity_data;
}

proto::AiThreadEntity SpecificsToEntityProto(
    const sync_pb::AiThreadSpecifics specifics) {
  proto::AiThreadEntity entity;
  entity.set_allocated_specifics(new sync_pb::AiThreadSpecifics(specifics));
  return entity;
}

void ApplyEntityProtoToTrimmedSpecifics(
    const proto::AiThreadEntity& entity,
    sync_pb::AiThreadSpecifics* mutable_base_specifics) {
  mutable_base_specifics->set_type(entity.specifics().type());
  mutable_base_specifics->set_server_id(entity.specifics().server_id());
  mutable_base_specifics->set_conversation_turn_id(
      entity.specifics().conversation_turn_id());
  mutable_base_specifics->set_title(entity.specifics().title());
}

}  // namespace

AiThreadSyncBridge::AiThreadSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::AI_THREAD,
           base::BindOnce(&AiThreadSyncBridge::OnDataTypeStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

AiThreadSyncBridge::~AiThreadSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
AiThreadSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> AiThreadSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_change_list));
}

std::optional<syncer::ModelError>
AiThreadSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      data_type_store_->CreateWriteBatch();
  std::vector<proto::AiThreadEntity> added_or_updated;
  std::vector<base::Uuid> removed;
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const sync_pb::EntitySpecifics& entity_specifics = change->data().specifics;

    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        CHECK(entity_specifics.has_ai_thread());
        const proto::AiThreadEntity& entity =
            SpecificsToEntityProto(entity_specifics.ai_thread());
        ai_thread_entities_[change->storage_key()] = entity;
        batch->WriteData(change->storage_key(), entity.SerializeAsString());
        added_or_updated.emplace_back(entity);
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        ai_thread_entities_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        removed.emplace_back(
            base::Uuid::ParseCaseInsensitive(change->storage_key()));
        break;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  data_type_store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&AiThreadSyncBridge::OnDataTypeStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));

  for (auto& observer : observers_) {
    observer.OnThreadAddedOrUpdatedRemotely(added_or_updated);
    observer.OnThreadRemovedRemotely(removed);
  }
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> AiThreadSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& key : storage_keys) {
    auto it = ai_thread_entities_.find(key);
    if (it != ai_thread_entities_.end()) {
      auto entity_data = std::make_unique<syncer::EntityData>();
      entity_data->specifics =
          change_processor()->GetPossiblyTrimmedRemoteSpecifics(key);
      ApplyEntityProtoToTrimmedSpecifics(
          it->second, entity_data->specifics.mutable_ai_thread());
      batch->Put(key, std::move(entity_data));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
AiThreadSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [server_id, entity] : ai_thread_entities_) {
    batch->Put(server_id, CreateEntityDataFromSpecifics(entity.specifics()));
  }
  return batch;
}

std::string AiThreadSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string AiThreadSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.ai_thread().server_id();
}

void AiThreadSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  std::vector<base::Uuid> uuids;
  for (const auto& [server_id, entity] : ai_thread_entities_) {
    uuids.push_back(base::Uuid::ParseCaseInsensitive(server_id));
  }
  ai_thread_entities_.clear();
  data_type_store_->DeleteAllDataAndMetadata(base::DoNothing());
  weak_ptr_factory_.InvalidateWeakPtrs();

  for (auto& observer : observers_) {
    observer.OnThreadRemovedRemotely(uuids);
  }
}

bool AiThreadSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return !entity_data.specifics.ai_thread().server_id().empty();
}

sync_pb::EntitySpecifics
AiThreadSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  // LINT.IfChange(TrimAllSupportedFieldsFromRemoteSpecifics)
  sync_pb::AiThreadSpecifics trimmed_specifics = entity_specifics.ai_thread();
  trimmed_specifics.clear_type();
  trimmed_specifics.clear_server_id();
  trimmed_specifics.clear_conversation_turn_id();
  trimmed_specifics.clear_title();
  // LINT.ThenChange(//components/sync/protocol/ai_thread_specifics.proto:AiThreadSpecifics)

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  if (trimmed_specifics.ByteSizeLong() > 0) {
    *trimmed_entity_specifics.mutable_ai_thread() =
        std::move(trimmed_specifics);
  }
  return trimmed_entity_specifics;
}

std::optional<Thread> AiThreadSyncBridge::GetThread(
    const std::string& server_id) const {
  auto it = ai_thread_entities_.find(server_id);
  if (it == ai_thread_entities_.end()) {
    return std::nullopt;
  }
  sync_pb::AiThreadSpecifics specifics = it->second.specifics();
  return Thread(ThreadType::kAiMode, server_id, specifics.title(),
                specifics.conversation_turn_id());
}

void AiThreadSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AiThreadSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AiThreadSyncBridge::OnDataTypeStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  data_type_store_ = std::move(store);

  data_type_store_->ReadAllData(base::BindOnce(
      &AiThreadSyncBridge::OnReadAllData, weak_ptr_factory_.GetWeakPtr()));
}

void AiThreadSyncBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const auto& record : *entries) {
    proto::AiThreadEntity entity;
    if (!entity.ParseFromString(record.value)) {
      change_processor()->ReportError(*error);
      return;
    }
    ai_thread_entities_[entity.specifics().server_id()] = std::move(entity);
  }

  is_data_loaded_ = true;
  for (auto& observer : observers_) {
    observer.OnThreadDataStoreLoaded();
  }

  data_type_store_->ReadAllMetadata(base::BindOnce(
      &AiThreadSyncBridge::OnReadAllMetadata, weak_ptr_factory_.GetWeakPtr()));
}

void AiThreadSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void AiThreadSyncBridge::OnDataTypeStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace contextual_tasks
