// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/types/optional_util.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace accessibility_annotator {

AccessibilityAnnotationSyncBridge::AccessibilityAnnotationSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::ACCESSIBILITY_ANNOTATION,
           base::BindOnce(
               &AccessibilityAnnotationSyncBridge::OnDataTypeStoreCreated,
               weak_ptr_factory_.GetWeakPtr()));
}

AccessibilityAnnotationSyncBridge::~AccessibilityAnnotationSyncBridge() =
    default;

std::unique_ptr<syncer::MetadataChangeList>
AccessibilityAnnotationSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
AccessibilityAnnotationSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_change_list));
}

std::optional<syncer::ModelError>
AccessibilityAnnotationSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      data_type_store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const sync_pb::EntitySpecifics& entity_specifics = change->data().specifics;

    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        CHECK(entity_specifics.has_accessibility_annotation());
        annotation_entries_[change->storage_key()] =
            entity_specifics.accessibility_annotation();
        batch->WriteData(
            change->storage_key(),
            entity_specifics.accessibility_annotation().SerializeAsString());
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        annotation_entries_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        break;
    }
  }
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  data_type_store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&AccessibilityAnnotationSyncBridge::OnDataTypeStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
AccessibilityAnnotationSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  // Accessibility annotations are read-only from the server.
  return nullptr;
}

std::unique_ptr<syncer::DataBatch>
AccessibilityAnnotationSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [id, specifics] : annotation_entries_) {
    auto entity_data = std::make_unique<syncer::EntityData>();
    entity_data->name = id;
    *entity_data->specifics.mutable_accessibility_annotation() = specifics;
    batch->Put(id, std::move(entity_data));
  }
  return batch;
}

std::string AccessibilityAnnotationSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string AccessibilityAnnotationSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.accessibility_annotation().id();
}

void AccessibilityAnnotationSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  annotation_entries_.clear();
  data_type_store_->DeleteAllDataAndMetadata(base::DoNothing());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool AccessibilityAnnotationSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return !entity_data.specifics.accessibility_annotation().id().empty();
}

std::optional<sync_pb::AccessibilityAnnotationSpecifics>
AccessibilityAnnotationSyncBridge::GetAnnotation(std::string_view id) const {
  return base::OptionalFromPtr(base::FindOrNull(annotation_entries_, id));
}

std::vector<sync_pb::AccessibilityAnnotationSpecifics>
AccessibilityAnnotationSyncBridge::GetAllAnnotations() const {
  return base::ToVector(annotation_entries_,
                        [](const auto& p) { return p.second; });
}

void AccessibilityAnnotationSyncBridge::OnDataTypeStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  data_type_store_ = std::move(store);

  data_type_store_->ReadAllData(
      base::BindOnce(&AccessibilityAnnotationSyncBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessibilityAnnotationSyncBridge::OnReadAllData(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  // Populate the in-memory cache from the database on startup.
  for (const syncer::DataTypeStore::Record& record : *entries) {
    sync_pb::AccessibilityAnnotationSpecifics specifics;
    if (specifics.ParseFromString(record.value)) {
      annotation_entries_[record.id] = std::move(specifics);
    }
  }

  data_type_store_->ReadAllMetadata(
      base::BindOnce(&AccessibilityAnnotationSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessibilityAnnotationSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  for (auto& observer : observers_) {
    observer.OnAccessibilityAnnotationSyncBridgeLoaded();
  }
}

void AccessibilityAnnotationSyncBridge::OnDataTypeStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void AccessibilityAnnotationSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityAnnotationSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace accessibility_annotator
