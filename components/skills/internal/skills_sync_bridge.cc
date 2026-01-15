// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_sync_bridge.h"

#include <memory>
#include <utility>

#include "base/notimplemented.h"
#include "base/uuid.h"
#include "components/skills/proto/skill_local_data.pb.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/skill_specifics.pb.h"

namespace skills {

namespace {

sync_pb::SkillSpecifics SkillToSpecifics(const Skill& skill) {
  // Skill ID must be a valid GUID.
  CHECK(base::Uuid::ParseLowercase(skill.id).is_valid());

  sync_pb::SkillSpecifics specifics;
  specifics.set_guid(skill.id);
  specifics.set_name(skill.name);
  specifics.set_icon(skill.icon);
  specifics.mutable_simple_skill()->set_prompt(skill.prompt);
  // TODO(crbug.com/471795213): support other fields once available.
  return specifics;
}

std::unique_ptr<Skill> SpecificsToSkill(
    const sync_pb::SkillSpecifics& specifics) {
  CHECK(base::Uuid::ParseLowercase(specifics.guid()).is_valid());

  return std::make_unique<Skill>(/*id=*/specifics.guid(),
                                 /*name=*/specifics.name(),
                                 /*icon=*/specifics.icon(),
                                 /*prompt=*/specifics.simple_skill().prompt());
}

syncer::EntityData SpecificsToEntityData(
    const sync_pb::SkillSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_skill() = specifics;
  entity_data.name = specifics.guid();
  return entity_data;
}

}  // namespace

SkillsSyncBridge::SkillsSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    SkillsService& skills_service)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      skills_service_(skills_service) {
  std::move(create_store_callback)
      .Run(syncer::SKILL, base::BindOnce(&SkillsSyncBridge::OnStoreCreated,
                                         weak_ptr_factory_.GetWeakPtr()));
}

SkillsSyncBridge::~SkillsSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

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

  // LINT.IfChange(TrimAllSupportedFieldsFromRemoteSpecifics)
  sync_pb::SkillSpecifics trimmed_specifics = entity_specifics.skill();
  trimmed_specifics.clear_guid();
  trimmed_specifics.clear_name();
  trimmed_specifics.clear_icon();
  trimmed_specifics.clear_creation_time_windows_epoch_micros();
  trimmed_specifics.clear_last_update_time_windows_epoch_micros();
  trimmed_specifics.clear_schema_version();

  if (trimmed_specifics.has_simple_skill()) {
    trimmed_specifics.mutable_simple_skill()->clear_prompt();

    if (trimmed_specifics.simple_skill().ByteSizeLong() == 0) {
      trimmed_specifics.clear_simple_skill();
    }
  }
  // LINT.ThenChange(//components/sync/protocol/skill_specifics.proto:SkillSpecifics)

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  if (trimmed_specifics.ByteSizeLong() > 0) {
    *trimmed_entity_specifics.mutable_skill() = std::move(trimmed_specifics);
  }

  return trimmed_entity_specifics;
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

void SkillsSyncBridge::OnSkillUpdated(const std::string& skill_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/471795213): verify that the changes are not coming from
  // sync.
  CHECK(store_);
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  const Skill* skill = skills_service_->GetSkillById(skill_id);
  if (!skill) {
    // Skill was deleted locally.
    batch->DeleteData(skill_id);
  } else {
    // Skill was created or updated locally.
    proto::SkillLocalData local_data;
    *local_data.mutable_specifics() = SkillToSpecifics(*skill);
    batch->WriteData(skill_id, local_data.SerializeAsString());
  }

  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&SkillsSyncBridge::OnDatabaseSave,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void SkillsSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  std::vector<std::unique_ptr<Skill>> loaded_skills;
  for (const syncer::DataTypeStore::Record& record : *entries) {
    proto::SkillLocalData local_data;
    if (!local_data.ParseFromString(record.value)) {
      // TODO(crbug.com/471795213): record invalid data histogram.
      continue;
    }
    if (!IsEntityDataValid(SpecificsToEntityData(local_data.specifics()))) {
      // TODO(crbug.com/471795213): record invalid data histogram.
      continue;
    }
    if (record.id != local_data.specifics().guid()) {
      // TODO(crbug.com/471795213): record invalid data histogram.
      continue;
    }

    loaded_skills.push_back(SpecificsToSkill(local_data.specifics()));
  }

  skills_service_->LoadInitialSkills(std::move(loaded_skills));
  skills_service_observation_.Observe(&skills_service_.get());

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void SkillsSyncBridge::OnDatabaseSave(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace skills
