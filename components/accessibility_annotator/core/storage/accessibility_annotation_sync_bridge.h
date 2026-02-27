// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATION_SYNC_BRIDGE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATION_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_change_list.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
class DataBatch;
}  // namespace syncer

namespace accessibility_annotator {

// Sync bridge implementation for accessibility annotation data type.
class AccessibilityAnnotationSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;
    // TODO(crbug.com/486856790): Add observer methods for
    // adding/removing/updating annotations.

    // Invoked when the store containing the accessibility annotations is
    // loaded.
    virtual void OnAccessibilityAnnotationSyncBridgeLoaded() = 0;
  };

  explicit AccessibilityAnnotationSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);

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

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // TODO(crbug.com/486856790): Consider converting the proto to a class/struct
  // and enforce validity and additional variants. Maybe do this in the cache
  // directly instead of the methods here.
  // Returns the annotation with the given ID, or nullopt if it is not found.
  std::optional<sync_pb::AccessibilityAnnotationSpecifics> GetAnnotation(
      std::string_view id) const;

  // Returns all annotations in the store.
  std::vector<sync_pb::AccessibilityAnnotationSpecifics> GetAllAnnotations()
      const;

 private:
  void OnDataTypeStoreCreated(const std::optional<syncer::ModelError>& error,
                              std::unique_ptr<syncer::DataTypeStore> store);

  void OnReadAllData(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries);

  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnDataTypeStoreCommit(const std::optional<syncer::ModelError>& error);

  std::unique_ptr<syncer::DataTypeStore> data_type_store_;

  absl::flat_hash_map<std::string, sync_pb::AccessibilityAnnotationSpecifics>
      annotation_entries_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<AccessibilityAnnotationSyncBridge> weak_ptr_factory_{
      this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATION_SYNC_BRIDGE_H_
