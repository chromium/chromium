// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_INTERNAL_SKILLS_SYNC_BRIDGE_H_
#define COMPONENTS_SKILLS_INTERNAL_SKILLS_SYNC_BRIDGE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace syncer {
class MetadataBatch;
class MetadataChangeList;
class DataTypeLocalChangeProcessor;
class DataTypeStore;
}  // namespace syncer

namespace skills {

class SkillsService;

// Sync bridge for the `SKILL` data type.
class SkillsSyncBridge : public syncer::DataTypeSyncBridge,
                         public SkillsService::Observer {
 public:
  SkillsSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory create_store_callback,
      SkillsService& skills_service);
  SkillsSyncBridge(const SkillsSyncBridge&) = delete;
  SkillsSyncBridge& operator=(const SkillsSyncBridge&) = delete;
  ~SkillsSyncBridge() override;

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
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
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  // SkillsService::Observer implementation.
  void OnSkillUpdated(std::string_view skill_id,
                      SkillsService::UpdateSource update_source,
                      bool is_position_changed) override;

 private:
  // Loads the data already stored in the DataTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);

  // Handle data loaded from the DataTypeStore.
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  // Called on database save completion.
  void OnDatabaseSave(const std::optional<syncer::ModelError>& error);

  // Returns the trimmed specifics for `storage_key` which may contain unknown
  // fields.
  const sync_pb::SkillSpecifics& GetPossiblyTrimmedSpecifics(
      const std::string& storage_key) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::DataTypeStore> store_;

  // The SkillsService that stores the data.
  base::raw_ref<SkillsService> skills_service_;

  base::ScopedObservation<SkillsService, SkillsService::Observer>
      skills_service_observation_{this};

  // Allows safe temporary use of the SkillsSyncBridge object if it exists at
  // the time of use.
  base::WeakPtrFactory<SkillsSyncBridge> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_SKILLS_SYNC_BRIDGE_H_
