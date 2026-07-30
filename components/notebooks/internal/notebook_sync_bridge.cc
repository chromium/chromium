// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebook_sync_bridge.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"

namespace notebooks {

namespace {

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::NotebookSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_notebook() = specifics;
  entity_data->name = specifics.uuid();
  return entity_data;
}

std::optional<Notebook> SpecificsToNotebook(
    const sync_pb::NotebookSpecifics& specifics) {
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(specifics.uuid());
  if (!uuid.is_valid()) {
    return std::nullopt;
  }
  base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.creation_time_windows_epoch_micros()));
  base::Time update_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.update_time_windows_epoch_micros()));
  return Notebook(NotebookId(uuid), creation_time, update_time);
}

}  // namespace

NotebookSyncBridge::NotebookSyncBridge(
    NotebooksModel* model,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      model_(CHECK_DEREF(model)) {
  std::move(store_factory)
      .Run(syncer::NOTEBOOK, base::BindOnce(&NotebookSyncBridge::OnStoreCreated,
                                            weak_ptr_factory_.GetWeakPtr()));
}

NotebookSyncBridge::~NotebookSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
NotebookSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> NotebookSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

std::optional<syncer::ModelError>
NotebookSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::EntitySpecifics& entity_specifics =
            change->data().specifics;
        // Guaranteed by ClientTagBasedDataTypeProcessor, based on
        // IsEntityDataValid().
        CHECK(entity_specifics.has_notebook());
        const sync_pb::NotebookSpecifics& specifics =
            entity_specifics.notebook();
        entries_[change->storage_key()] = specifics;
        batch->WriteData(change->storage_key(), specifics.SerializeAsString());
        if (std::optional<Notebook> notebook = SpecificsToNotebook(specifics)) {
          model_->AddOrUpdateNotebook(*std::move(notebook));
        } else {
          DLOG(WARNING) << "Failed to parse Notebook from specifics: "
                        << change->storage_key();
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE: {
        entries_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        base::Uuid uuid =
            base::Uuid::ParseCaseInsensitive(change->storage_key());
        if (uuid.is_valid()) {
          model_->RemoveNotebook(NotebookId(uuid));
        } else {
          DLOG(WARNING) << "Invalid storage key UUID on delete: "
                        << change->storage_key();
        }
        break;
      }
    }
  }

  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&NotebookSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> NotebookSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    if (const sync_pb::NotebookSpecifics* specifics =
            base::FindOrNull(entries_, key)) {
      batch->Put(key, CreateEntityData(*specifics));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
NotebookSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [storage_key, specifics] : entries_) {
    batch->Put(storage_key, CreateEntityData(specifics));
  }
  return batch;
}

std::string NotebookSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.notebook().uuid();
}

std::string NotebookSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTag(entity_data);
}

void NotebookSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.clear();
  store_->DeleteAllDataAndMetadata(std::move(delete_metadata_change_list),
                                   base::DoNothing());
}

bool NotebookSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !entity_data.specifics.notebook().uuid().empty();
}

sync_pb::EntitySpecifics
NotebookSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // LINT.IfChange(TrimAllSupportedFieldsFromRemoteSpecifics)
  sync_pb::NotebookSpecifics trimmed_specifics = entity_specifics.notebook();
  trimmed_specifics.clear_uuid();
  trimmed_specifics.clear_creation_time_windows_epoch_micros();
  trimmed_specifics.clear_update_time_windows_epoch_micros();
  trimmed_specifics.clear_notebook();
  trimmed_specifics.clear_schema_version();
  // LINT.ThenChange(//components/sync/protocol/notebook_specifics.proto:NotebookSpecifics)

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  if (trimmed_specifics.ByteSizeLong() > 0) {
    *trimmed_entity_specifics.mutable_notebook() = std::move(trimmed_specifics);
  }
  return trimmed_entity_specifics;
}

void NotebookSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&NotebookSyncBridge::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void NotebookSyncBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> records) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const syncer::DataTypeStore::Record& record : *records) {
    sync_pb::NotebookSpecifics specifics;
    if (!specifics.ParseFromString(record.value)) {
      DLOG(WARNING) << "Failed to parse NotebookSpecifics from record: "
                    << record.id;
      continue;
    }
    if (std::optional<Notebook> notebook = SpecificsToNotebook(specifics)) {
      model_->AddOrUpdateNotebook(*std::move(notebook));
    } else {
      DLOG(WARNING) << "Failed to parse Notebook from specifics: " << record.id;
    }
    entries_[record.id] = std::move(specifics);
  }

  store_->ReadAllMetadata(base::BindOnce(&NotebookSyncBridge::OnReadAllMetadata,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void NotebookSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  model_->SetLoaded();
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void NotebookSyncBridge::OnCommit(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace notebooks
