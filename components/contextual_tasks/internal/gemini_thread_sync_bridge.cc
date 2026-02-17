// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/gemini_thread_sync_bridge.h"

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
  // TODO(crbug.com/483958666) Implement
  NOTIMPLEMENTED();
  return nullptr;
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
  // TODO(crbug.com/483956034) Implement
  NOTIMPLEMENTED();
}

bool GeminiThreadSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return !entity_data.specifics.gemini_thread().conversation_id().empty();
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
  // TODO(crbug.com/483959855) Finish implementation

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

}  // namespace contextual_tasks
