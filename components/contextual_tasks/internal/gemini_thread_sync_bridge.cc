// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/gemini_thread_sync_bridge.h"

#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/uuid.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/gemini_thread_specifics.pb.h"

namespace {

std::unique_ptr<syncer::EntityData> CreateEntityDataFromSpecifics(
    const sync_pb::GeminiThreadSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_gemini_thread() = specifics;
  entity_data->name = specifics.conversation_id();
  return entity_data;
}

void ApplyEntityProtoToTrimmedSpecifics(
    const sync_pb::GeminiThreadSpecifics& specifics,
    sync_pb::GeminiThreadSpecifics* mutable_base_specifics) {
  mutable_base_specifics->set_conversation_id(specifics.conversation_id());
  mutable_base_specifics->set_title(specifics.title());
  mutable_base_specifics->set_last_turn_time_unix_epoch_millis(
      specifics.last_turn_time_unix_epoch_millis());
}

}  // namespace

namespace contextual_tasks {

GeminiThreadSyncBridge::GeminiThreadSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::GEMINI_THREAD,
           base::BindOnce(&GeminiThreadSyncBridge::OnDataTypeStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

GeminiThreadSyncBridge::~GeminiThreadSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
GeminiThreadSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> GeminiThreadSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_change_list));
}

std::optional<syncer::ModelError>
GeminiThreadSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      data_type_store_->CreateWriteBatch();
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const sync_pb::EntitySpecifics& entity_specifics = change->data().specifics;

    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        CHECK(entity_specifics.has_gemini_thread());
        gemini_thread_specifics_[change->storage_key()] =
            entity_specifics.gemini_thread();
        batch->WriteData(change->storage_key(),
                         entity_specifics.gemini_thread().SerializeAsString());
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        gemini_thread_specifics_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        break;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  data_type_store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&GeminiThreadSyncBridge::OnDataTypeStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> GeminiThreadSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& key : storage_keys) {
    auto it = gemini_thread_specifics_.find(key);
    if (it != gemini_thread_specifics_.end()) {
      auto entity_data = std::make_unique<syncer::EntityData>();
      entity_data->specifics =
          change_processor()->GetPossiblyTrimmedRemoteSpecifics(key);
      ApplyEntityProtoToTrimmedSpecifics(
          it->second, entity_data->specifics.mutable_gemini_thread());
      batch->Put(key, std::move(entity_data));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
GeminiThreadSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [conversation_id, specifics] : gemini_thread_specifics_) {
    batch->Put(conversation_id, CreateEntityDataFromSpecifics(specifics));
  }
  return batch;
}

std::string GeminiThreadSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string GeminiThreadSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.gemini_thread().conversation_id();
}

void GeminiThreadSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  gemini_thread_specifics_.clear();
  data_type_store_->DeleteAllDataAndMetadata(base::DoNothing());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool GeminiThreadSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return !entity_data.specifics.gemini_thread().conversation_id().empty();
}

sync_pb::EntitySpecifics
GeminiThreadSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  // LINT.IfChange(TrimAllSupportedFieldsFromRemoteSpecifics)
  sync_pb::GeminiThreadSpecifics trimmed_specifics =
      entity_specifics.gemini_thread();
  trimmed_specifics.clear_conversation_id();
  trimmed_specifics.clear_title();
  trimmed_specifics.clear_last_turn_time_unix_epoch_millis();
  // LINT.ThenChange(//components/sync/protocol/gemini_thread_specifics.proto)

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  if (trimmed_specifics.ByteSizeLong() > 0) {
    *trimmed_entity_specifics.mutable_gemini_thread() =
        std::move(trimmed_specifics);
  }
  return trimmed_entity_specifics;
}

void GeminiThreadSyncBridge::OnDataTypeStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  data_type_store_ = std::move(store);

  data_type_store_->ReadAllData(base::BindOnce(
      &GeminiThreadSyncBridge::OnReadAllData, weak_ptr_factory_.GetWeakPtr()));
}

void GeminiThreadSyncBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  for (const auto& record : *entries) {
    sync_pb::GeminiThreadSpecifics entity;
    if (!entity.ParseFromString(record.value)) {
      change_processor()->ReportError(*error);
      continue;
    }
    gemini_thread_specifics_[entity.conversation_id()] = std::move(entity);
  }
  for (auto& observer : observers_) {
    observer.OnGeminiThreadDataStoreLoaded();
  }

  data_type_store_->ReadAllMetadata(
      base::BindOnce(&GeminiThreadSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GeminiThreadSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void GeminiThreadSyncBridge::OnDataTypeStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

std::vector<Thread> GeminiThreadSyncBridge::GetThreads() const {
  std::vector<Thread> threads;
  for (const auto& [conversation_id, specifics] : gemini_thread_specifics_) {
    threads.emplace_back(ThreadType::kGemini, conversation_id,
                         specifics.title());
  }
  return threads;
}

void GeminiThreadSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GeminiThreadSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace contextual_tasks
