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
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/skill_specifics.pb.h"

namespace skills {

namespace {

constexpr int kSchemaVersion = 1;

base::Time FromWindowsEpochMicros(int64_t windows_epoch_micros) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(windows_epoch_micros));
}

int64_t ToWindowsEpochMicros(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Converts a `skill` to specifics proto. `base_specifics` is used to preserve
// unknown fields when committing to the server.
sync_pb::SkillSpecifics SkillToSpecifics(
    const Skill& skill,
    sync_pb::SkillSpecifics base_specifics) {
  sync_pb::SkillSpecifics specifics = std::move(base_specifics);
  specifics.set_guid(skill.id);
  specifics.set_source_skill_id(skill.source_skill_id);
  specifics.set_name(skill.name);
  specifics.set_icon(skill.icon);
  specifics.mutable_simple_skill()->set_prompt(skill.prompt);
  specifics.mutable_simple_skill()->set_description(skill.description);
  specifics.set_creation_time_windows_epoch_micros(
      ToWindowsEpochMicros(skill.creation_time));
  specifics.set_last_update_time_windows_epoch_micros(
      ToWindowsEpochMicros(skill.last_update_time));
  specifics.set_schema_version(kSchemaVersion);
  specifics.set_skill_source(skill.source);
  return specifics;
}

std::unique_ptr<Skill> SpecificsToSkill(
    const sync_pb::SkillSpecifics& specifics) {
  CHECK(specifics.has_simple_skill());

  auto skill = std::make_unique<Skill>(
      /*id=*/specifics.guid(),
      /*name=*/specifics.name(),
      /*icon=*/specifics.icon(),
      /*prompt=*/specifics.simple_skill().prompt(),
      /*description=*/specifics.simple_skill().description());
  if (!specifics.source_skill_id().empty()) {
    skill->source_skill_id = specifics.source_skill_id();
  }

  skill->creation_time =
      FromWindowsEpochMicros(specifics.creation_time_windows_epoch_micros());
  skill->last_update_time =
      FromWindowsEpochMicros(specifics.last_update_time_windows_epoch_micros());
  skill->source = specifics.skill_source();
  return skill;
}

syncer::EntityData SpecificsToEntityData(
    const sync_pb::SkillSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_skill() = specifics;
  entity_data.name = specifics.guid();
  return entity_data;
}

void StoreSkill(const Skill& skill,
                syncer::DataTypeStore::WriteBatch& write_batch) {
  proto::SkillLocalData local_data;
  // Do not store unknown fields to avoid duplicating data as it's already
  // stored in the sync metadata.
  *local_data.mutable_specifics() =
      SkillToSpecifics(skill, /*base_specifics=*/sync_pb::SkillSpecifics());
  write_batch.WriteData(skill.id, local_data.SerializeAsString());
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
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This data type does not store local-only data so the initial merge is the
  // same as an incremental update.
  std::optional<syncer::ModelError> error = ApplyIncrementalSyncChanges(
      std::move(metadata_change_list), std::move(entity_changes));
  skills_service_->SyncStatusChanged();

  return error;
}

std::optional<syncer::ModelError> SkillsSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  for (const std::unique_ptr<syncer::EntityChange>& entity_change :
       entity_changes) {
    switch (entity_change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::SkillSpecifics& skill_specifics =
            entity_change->data().specifics.skill();

        if (!skill_specifics.has_simple_skill()) {
          // This is a new type of skill, ignore it in the bridge but its
          // metadata should still be stored.
          continue;
        }

        const Skill* skill = skills_service_->AddOrUpdateSkillFromSync(
            skill_specifics.guid(), skill_specifics.source_skill_id(),
            skill_specifics.name(), skill_specifics.icon(),
            skill_specifics.simple_skill().prompt(),
            skill_specifics.simple_skill().description(),
            FromWindowsEpochMicros(
                skill_specifics.creation_time_windows_epoch_micros()),
            FromWindowsEpochMicros(
                skill_specifics.last_update_time_windows_epoch_micros()),
            skill_specifics.skill_source());
        CHECK(skill);

        StoreSkill(*skill, *write_batch);
        break;
      }

      case syncer::EntityChange::ACTION_DELETE: {
        const std::string& skill_id = entity_change->storage_key();
        skills_service_->DeleteSkill(skill_id,
                                     SkillsService::UpdateSource::kSync);
        write_batch->DeleteData(skill_id);
        break;
      }
    }
  }

  store_->CommitWriteBatch(std::move(write_batch),
                           base::BindOnce(&SkillsSyncBridge::OnDatabaseSave,
                                          weak_ptr_factory_.GetWeakPtr()));

  return {};
}

std::unique_ptr<syncer::DataBatch> SkillsSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& storage_key : storage_keys) {
    const Skill* skill = skills_service_->GetSkillById(storage_key);
    if (!skill) {
      // Skill was deleted locally.
      continue;
    }

    batch->Put(storage_key,
               std::make_unique<syncer::EntityData>(
                   SpecificsToEntityData(SkillToSpecifics(
                       *skill, GetPossiblyTrimmedSpecifics(storage_key)))));
  }

  return batch;
}

std::unique_ptr<syncer::DataBatch> SkillsSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::unique_ptr<Skill>& skill : skills_service_->GetSkills()) {
    batch->Put(
        skill->id,
        std::make_unique<syncer::EntityData>(SpecificsToEntityData(
            SkillToSpecifics(*skill,
                             /*base_specifics=*/sync_pb::SkillSpecifics()))));
  }

  return batch;
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

  std::vector<std::string> skills_to_delete;
  skills_to_delete.reserve(skills_service_->GetSkills().size());
  for (const std::unique_ptr<Skill>& skill : skills_service_->GetSkills()) {
    skills_to_delete.push_back(skill->id);
  }

  for (const std::string& skill_id : skills_to_delete) {
    skills_service_->DeleteSkill(skill_id, SkillsService::UpdateSource::kSync);
  }

  // All skills must be deleted from the service.
  CHECK(skills_service_->GetSkills().empty());

  // Do not use `delete_metadata_change_list` as all data and metadata should be
  // deleted.
  store_->DeleteAllDataAndMetadata(base::BindOnce(
      &SkillsSyncBridge::OnDatabaseSave, weak_ptr_factory_.GetWeakPtr()));

  skills_service_->SyncStatusChanged();
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
  trimmed_specifics.clear_skill_source();
  trimmed_specifics.clear_source_skill_id();

  if (trimmed_specifics.has_simple_skill()) {
    trimmed_specifics.mutable_simple_skill()->clear_prompt();
    trimmed_specifics.mutable_simple_skill()->clear_description();

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

  // Do not validate the presence of `simple_skill` for forward compatibility
  // with the newer version which may contain other skill types. The bridge will
  // not propagate those changes to the server but it would be stored in sync
  // metadata to detect unknown fields and potentially redownload the data.

  return true;
}

void SkillsSyncBridge::OnSkillUpdated(std::string_view skill_id,
                                      SkillsService::UpdateSource update_source,
                                      bool is_position_changed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(store_);
  // TODO(crbug.com/471795213): consider using CHECK for tracking metadata.

  if (update_source == SkillsService::UpdateSource::kSync) {
    // This change was made by the remote sync service and it should not be sent
    // back to the sync service.
    return;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  const Skill* skill = skills_service_->GetSkillById(skill_id);
  if (!skill) {
    // Skill was deleted locally.
    std::string skill_id_str(skill_id.data());
    batch->DeleteData(skill_id_str);
    change_processor()->Delete(skill_id_str,
                               syncer::DeletionOrigin::Unspecified(),
                               batch->GetMetadataChangeList());
  } else {
    // Skill was created or updated locally.
    StoreSkill(*skill, *batch);
    change_processor()->Put(
        skill->id,
        std::make_unique<syncer::EntityData>(SpecificsToEntityData(
            SkillToSpecifics(*skill, GetPossiblyTrimmedSpecifics(skill->id)))),
        batch->GetMetadataChangeList());
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
      continue;
    }
    if (!IsEntityDataValid(SpecificsToEntityData(local_data.specifics()))) {
      continue;
    }
    if (!local_data.specifics().has_simple_skill()) {
      // Verify this explicitly as `IsEntityDataValid()` does not check for it.
      continue;
    }
    if (record.id != local_data.specifics().guid()) {
      continue;
    }

    loaded_skills.push_back(SpecificsToSkill(local_data.specifics()));
  }

  skills_service_->LoadInitialSkills(std::move(loaded_skills));
  skills_service_observation_.Observe(&skills_service_.get());

  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  if (change_processor()->IsTrackingMetadata()) {
    skills_service_->SyncStatusChanged();
  }
}

void SkillsSyncBridge::OnDatabaseSave(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
  }
}

const sync_pb::SkillSpecifics& SkillsSyncBridge::GetPossiblyTrimmedSpecifics(
    const std::string& storage_key) const {
  // TODO(crbug.com/471795213): verify that metadata is being tracked.
  if (!change_processor()->IsTrackingMetadata()) {
    return sync_pb::SkillSpecifics::default_instance();
  }

  return change_processor()
      ->GetPossiblyTrimmedRemoteSpecifics(storage_key)
      .skill();
}

}  // namespace skills
