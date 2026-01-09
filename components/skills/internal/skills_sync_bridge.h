// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_INTERNAL_SKILLS_SYNC_BRIDGE_H_
#define COMPONENTS_SKILLS_INTERNAL_SKILLS_SYNC_BRIDGE_H_

#include <memory>

#include "base/sequence_checker.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace syncer {
class MetadataChangeList;
class DataTypeLocalChangeProcessor;
}  // namespace syncer

namespace skills {

// Sync bridge for the `SKILL` data type.
class SkillsSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  explicit SkillsSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);
  SkillsSyncBridge(const SkillsSyncBridge&) = delete;
  SkillsSyncBridge& operator=(const SkillsSyncBridge&) = delete;
  ~SkillsSyncBridge() override;

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
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

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_SKILLS_SYNC_BRIDGE_H_
