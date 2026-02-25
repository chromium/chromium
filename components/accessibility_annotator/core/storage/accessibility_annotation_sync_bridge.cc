// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"

namespace accessibility_annotator {

AccessibilityAnnotationSyncBridge::AccessibilityAnnotationSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : DataTypeSyncBridge(std::move(change_processor)) {}

AccessibilityAnnotationSyncBridge::~AccessibilityAnnotationSyncBridge() =
    default;

std::unique_ptr<syncer::MetadataChangeList>
AccessibilityAnnotationSyncBridge::CreateMetadataChangeList() {
  return nullptr;
}

std::optional<syncer::ModelError>
AccessibilityAnnotationSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  return std::nullopt;
}

std::optional<syncer::ModelError>
AccessibilityAnnotationSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
AccessibilityAnnotationSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  return nullptr;
}

std::unique_ptr<syncer::DataBatch>
AccessibilityAnnotationSyncBridge::GetAllDataForDebugging() {
  return nullptr;
}

std::string AccessibilityAnnotationSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return std::string();
}

std::string AccessibilityAnnotationSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return std::string();
}

void AccessibilityAnnotationSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {}

bool AccessibilityAnnotationSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return false;
}

}  // namespace accessibility_annotator
