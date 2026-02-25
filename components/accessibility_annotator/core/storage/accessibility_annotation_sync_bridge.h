// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATION_SYNC_BRIDGE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATION_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_change_list.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
class DataBatch;
}  // namespace syncer

namespace accessibility_annotator {

// Sync bridge implementation for accessibility annotation data type.
class AccessibilityAnnotationSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  explicit AccessibilityAnnotationSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  AccessibilityAnnotationSyncBridge(const AccessibilityAnnotationSyncBridge&) =
      delete;
  AccessibilityAnnotationSyncBridge& operator=(
      const AccessibilityAnnotationSyncBridge&) = delete;
  AccessibilityAnnotationSyncBridge(AccessibilityAnnotationSyncBridge&&) =
      delete;
  AccessibilityAnnotationSyncBridge& operator=(
      AccessibilityAnnotationSyncBridge&&) = delete;

  ~AccessibilityAnnotationSyncBridge() override;

  // DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_change_list) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATION_SYNC_BRIDGE_H_
