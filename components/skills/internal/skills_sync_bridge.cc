// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_sync_bridge.h"

#include <memory>
#include <utility>

#include "base/notimplemented.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/skill_specifics.pb.h"

namespace skills {

SkillsSyncBridge::SkillsSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory create_store_callback)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(create_store_callback)
      .Run(syncer::SKILL, base::BindOnce(&SkillsSyncBridge::OnStoreCreated,
                                         weak_ptr_factory_.GetWeakPtr()));
}

SkillsSyncBridge::~SkillsSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
SkillsSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> SkillsSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This data type does not store local-only data so the initial merge is the
  // same as an incremental update.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError> SkillsSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return {};
}

std::unique_ptr<syncer::DataBatch> SkillsSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<syncer::DataBatch> SkillsSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

std::string SkillsSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.skill().guid();
}

std::string SkillsSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return GetClientTag(entity_data);
}

void SkillsSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return;
}

sync_pb::EntitySpecifics
SkillsSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return {};
}

bool SkillsSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (GetClientTag(entity_data).empty()) {
    return false;
  }

  const sync_pb::SkillSpecifics& skill_specifics =
      entity_data.specifics.skill();
  if (skill_specifics.guid().empty()) {
    return false;
  }

  if (!skill_specifics.has_simple_skill()) {
    return false;
  }

  return true;
}

void SkillsSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&SkillsSyncBridge::OnReadAllDataAndMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SkillsSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  NOTIMPLEMENTED();

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

}  // namespace skills
