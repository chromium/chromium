// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/shared_tab_group_account_data_sync_bridge.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_account_data_specifics.pb.h"

namespace tab_groups {
namespace {

// Convert proto int64 microseconds since Windows-epoch to base::Time.
base::Time DeserializeTime(int64_t proto_time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_time));
}

// Convert base::Time to proto int64 microseconds since Windows-epoch.
int64_t SerializeTime(const base::Time& t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Client tag consists of the tab guid concatenated with collaboration id.
std::string CreateClientTagForSharedTab(const SavedTabGroup& group,
                                        const SavedTabGroupTab& tab) {
  return tab.saved_tab_guid().AsLowercaseString() + "|" +
         group.collaboration_id().value().value();
}

// Client tag consists of the tab guid concatenated with collaboration id.
std::string CreateClientTagForSharedTab(const CollaborationId& collaboration_id,
                                        const base::Uuid& tab_guid) {
  return tab_guid.AsLowercaseString() + "|" + collaboration_id.value();
}

std::string CreateClientTagForSharedGroup(const SavedTabGroup& group) {
  return group.saved_guid().AsLowercaseString() + "|" +
         group.collaboration_id().value().value();
}

// Returns the client tag for this specifics object. Note that
// SharedTabGroupAccountDataSpecifics uses the client tag as a storage key.
std::string GetClientTagFromSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  return specifics.guid() + "|" + specifics.collaboration_id();
}

// Trim specifics for use in TrimAllSupportedFieldsFromRemoteSpecifics.
// LINT.IfChange(TrimSpecifics)
sync_pb::SharedTabGroupAccountDataSpecifics TrimSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics& account_specifics) {
  sync_pb::SharedTabGroupAccountDataSpecifics trimmed_account_specifics =
      sync_pb::SharedTabGroupAccountDataSpecifics(account_specifics);
  trimmed_account_specifics.clear_guid();
  trimmed_account_specifics.clear_collaboration_id();
  trimmed_account_specifics.clear_update_time_windows_epoch_micros();
  trimmed_account_specifics.clear_version();

  if (trimmed_account_specifics.has_shared_tab_details()) {
    sync_pb::SharedTabDetails* tab =
        trimmed_account_specifics.mutable_shared_tab_details();
    tab->clear_shared_tab_group_guid();
    tab->clear_last_seen_timestamp_windows_epoch();

    if (tab->ByteSizeLong() == 0) {
      trimmed_account_specifics.clear_shared_tab_details();
    }
  }

  if (trimmed_account_specifics.has_shared_tab_group_details()) {
    sync_pb::SharedTabGroupDetails* tab_group =
        trimmed_account_specifics.mutable_shared_tab_group_details();
    tab_group->clear_pinned_position();

    if (tab_group->ByteSizeLong() == 0) {
      trimmed_account_specifics.clear_shared_tab_group_details();
    }
  }

  return trimmed_account_specifics;
}
// LINT.ThenChange(//components/sync/protocol/shared_tab_group_account_data_specifics.proto:SharedTabGroupAccountDataSpecifics)

// Create new EntityData object to contain specifics for writing changes.
std::unique_ptr<syncer::EntityData> CreateEntityDataFromSpecifics(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_shared_tab_group_account_data() = specifics;
  entity_data->name = specifics.guid();
  return entity_data;
}

bool SharedTabExistsForSpecifics(
    const SavedTabGroupModel& model,
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  // Can only be called with specifics containing TabDetails.
  CHECK(specifics.has_shared_tab_details());

  const base::Uuid group_id = base::Uuid::ParseCaseInsensitive(
      specifics.shared_tab_details().shared_tab_group_guid());
  const SavedTabGroup* group = model.Get(group_id);
  if (!group) {
    return false;
  }

  const base::Uuid tab_id = base::Uuid::ParseCaseInsensitive(specifics.guid());
  return group->GetTab(tab_id) != nullptr;
}

bool IsTabDetailsValid(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  if (!specifics.has_shared_tab_details()) {
    // Non-tab account specifics should be handled here.
    return false;
  }

  const sync_pb::SharedTabDetails& tab_details = specifics.shared_tab_details();
  if (!base::Uuid::ParseCaseInsensitive(tab_details.shared_tab_group_guid())
           .is_valid() ||
      !tab_details.has_last_seen_timestamp_windows_epoch()) {
    return false;
  }

  return true;
}

bool IsTabGroupDetailsValid(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  return specifics.has_shared_tab_group_details();
}

}  // namespace

SharedTabGroupAccountDataSyncBridge::SharedTabGroupAccountDataSyncBridge(
    std::unique_ptr<SyncDataTypeConfiguration> configuration,
    SavedTabGroupModel& model)
    : syncer::DataTypeSyncBridge(std::move(configuration->change_processor)),
      model_(model) {
  std::move(configuration->data_type_store_factory)
      .Run(syncer::SHARED_TAB_GROUP_ACCOUNT_DATA,
           base::BindOnce(&SharedTabGroupAccountDataSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));

  saved_tab_group_model_observation_.Observe(&model);
}

SharedTabGroupAccountDataSyncBridge::~SharedTabGroupAccountDataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
SharedTabGroupAccountDataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
SharedTabGroupAccountDataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Since this data type is controlled along with shared tab group data,
  // there will never be any shared tab groups in the model, therefore no
  // data to merge, when this data type is enabled.

  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_change_list));
}

std::optional<syncer::ModelError>
SharedTabGroupAccountDataSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change :
       entity_change_list) {
    const sync_pb::EntitySpecifics& entity_specifics = change->data().specifics;

    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        CHECK(entity_specifics.has_shared_tab_group_account_data());
        const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
            entity_specifics.shared_tab_group_account_data();

        specifics_[change->storage_key()] = specifics;
        batch->WriteData(change->storage_key(), specifics.SerializeAsString());

        // Only update the model after storing the change in the cache.
        // The model listeners will fire and will only no-op if they can
        // find the specifics in-memory.
        if (specifics.has_shared_tab_details()) {
          UpdateTabDetailsModel(specifics);
        } else if (specifics.has_shared_tab_group_details()) {
          UpdateTabGroupDetailsModel(specifics);
        }

        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        specifics_.erase(change->storage_key());
        batch->DeleteData(change->storage_key());
        // In case this was for shared_tab_details, delete from missing
        // tabs cache.
        storage_keys_for_missing_tabs_.erase(change->storage_key());
        break;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(
          &SharedTabGroupAccountDataSyncBridge::OnDataTypeStoreCommit,
          weak_ptr_factory_.GetWeakPtr()));

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
SharedTabGroupAccountDataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    if (specifics_.contains(storage_key)) {
      batch->Put(storage_key,
                 CreateEntityDataFromSpecifics(specifics_[storage_key]));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
SharedTabGroupAccountDataSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [storage_key, specifics] : specifics_) {
    batch->Put(storage_key, CreateEntityDataFromSpecifics(specifics));
  }
  return batch;
}

std::string SharedTabGroupAccountDataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTagFromSpecifics(
      entity_data.specifics.shared_tab_group_account_data());
}

std::string SharedTabGroupAccountDataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTag(entity_data);
}

bool SharedTabGroupAccountDataSyncBridge::SupportsGetClientTag() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool SharedTabGroupAccountDataSyncBridge::SupportsGetStorageKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void SharedTabGroupAccountDataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_keys_for_missing_tabs_.clear();
  specifics_.clear();
  store_->DeleteAllDataAndMetadata(base::DoNothing());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool SharedTabGroupAccountDataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
      entity_data.specifics.shared_tab_group_account_data();
  if (!base::Uuid::ParseCaseInsensitive(specifics.guid()).is_valid() ||
      specifics.collaboration_id().empty()) {
    return false;
  }

  return IsTabDetailsValid(specifics) || IsTabGroupDetailsValid(specifics);
}

sync_pb::EntitySpecifics
SharedTabGroupAccountDataSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const sync_pb::SharedTabGroupAccountDataSpecifics trimmed_specifics =
      TrimSpecifics(entity_specifics.shared_tab_group_account_data());

  if (trimmed_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_shared_tab_group_account_data() =
      trimmed_specifics;
  return trimmed_entity_specifics;
}

syncer::ConflictResolution SharedTabGroupAccountDataSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const syncer::EntityData& remote_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we are not tracking this storage_key, accept the remote change.
  if (!specifics_.contains(storage_key)) {
    return syncer::ConflictResolution::kUseRemote;
  }

  const sync_pb::SharedTabGroupAccountDataSpecifics local_specifics =
      specifics_.at(storage_key);
  const sync_pb::SharedTabGroupAccountDataSpecifics remote_specifics =
      remote_data.specifics.shared_tab_group_account_data();

  // The account data specifics can contain different types of details.
  // If these specifics both contain TabDetails, compare the timestamps
  // to see which one should be retained.
  if (local_specifics.has_shared_tab_details() &&
      remote_specifics.has_shared_tab_details()) {
    const int64_t local_timestamp = local_specifics.shared_tab_details()
                                        .last_seen_timestamp_windows_epoch();
    const int64_t remote_timestamp = remote_specifics.shared_tab_details()
                                         .last_seen_timestamp_windows_epoch();

    if (local_timestamp == remote_timestamp) {
      // Timestamps match.
      return syncer::ConflictResolution::kChangesMatch;
    } else if (local_timestamp > remote_timestamp) {
      // A local change has been Put to sync containing the more recent
      // time. Discard the remote change as it is outdated.
      return syncer::ConflictResolution::kUseLocal;
    } else {
      // A remote change contains a more recent timestamp than a change
      // that was Put locally. Discard the outdated local change.
      return syncer::ConflictResolution::kUseRemote;
    }
  }

  return syncer::ConflictResolution::kUseRemote;
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupModelLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ApplyMissingTabData();
  ApplyMissingTabGroupData();
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupTabLastSeenTimeUpdated(
    const base::Uuid& saved_tab_id,
    TriggerSource source) {
  if (source != TriggerSource::LOCAL) {
    return;
  }

  if (!is_initialized_ || !change_processor()->IsTrackingMetadata()) {
    // Ignore any changes before the model is successfully initialized.
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<syncer::EntityData>> new_entities;

  // Look through all tabs in this group and create entities for changes
  // that are not synced.
  const SavedTabGroup* group = model_->GetGroupContainingTab(saved_tab_id);
  if (!group || !group->is_shared_tab_group()) {
    return;
  }

  const SavedTabGroupTab* tab = group->GetTab(saved_tab_id);
  CHECK(tab);

  const std::optional<base::Time>& model_last_seen = tab->last_seen_time();
  if (!model_last_seen.has_value()) {
    // This tab has not been seen by the user. Avoid syncing tabs
    // without a timestamp by skipping this.
    return;
  }

  const std::string storage_key = CreateClientTagForSharedTab(*group, *tab);
  if (!specifics_.contains(storage_key)) {
    // This tab has been seen but does not exist in sync yet.
    WriteEntityToSync(CreateEntityDataFromSavedTabGroupTab(*model_, *tab));
    return;
  }

  const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
      specifics_.at(storage_key);
  const base::Time proto_last_seen = DeserializeTime(
      specifics.shared_tab_details().last_seen_timestamp_windows_epoch());

  if (proto_last_seen >= model_last_seen) {
    // Ignore the value if sync and model are up-to-date. Technically, it
    // should never be true that the model data is older than the value
    // in sync since we update it elsewhere, but this is also ignored.
    return;
  }

  // This tab was seen again. Update sync with the new timestamp.
  WriteEntityToSync(CreateEntityDataFromSavedTabGroupTab(*model_, *tab));
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  if (!is_initialized_ || !change_processor()->IsTrackingMetadata()) {
    // Ignore any changes before the model is successfully initialized.
    return;
  }

  const SavedTabGroup* group = model_->Get(group_guid);
  CHECK(group);
  if (!group->is_shared_tab_group()) {
    return;
  }

  if (tab_guid) {
    MaybeRemoveTabDetailsOnGroupUpdate(*group, tab_guid);
  } else {
    // Handle shared tab group details.
    WriteTabGroupDetailToSyncIfPositionChanged(*group);
  }
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  if (!is_initialized_) {
    // Ignore any changes before the model is successfully initialized.
    return;
  }

  const SavedTabGroup* group = model_->Get(group_guid);
  CHECK(group);
  if (!group->is_shared_tab_group()) {
    return;
  }

  MaybeRemoveTabDetailsOnGroupUpdate(*group, tab_guid);
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  if (!is_initialized_) {
    // Ignore any changes before the model is successfully initialized.
    return;
  }

  if (!removed_group.is_shared_tab_group()) {
    return;
  }

  // Delete tab entities for all tabs in the group.
  for (const SavedTabGroupTab& tab : removed_group.saved_tabs()) {
    RemoveEntitySpecifics(CreateClientTagForSharedTab(removed_group, tab));
  }

  // Remove tab group details entity.
  RemoveEntitySpecifics(CreateClientTagForSharedGroup(removed_group));
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  SavedTabGroupRemovedLocally(removed_group);
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  if (!is_initialized_ || !change_processor()->IsTrackingMetadata()) {
    // Ignore any changes before the model is successfully initialized.
    return;
  }

  const SavedTabGroup* group = model_->Get(guid);

  if (!group || !group->is_shared_tab_group()) {
    return;
  }

  WriteTabGroupDetailToSyncIfPositionChanged(*group);
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  if (!is_initialized_) {
    // Ignore any changes before the model is successfully initialized.
    return;
  }

  const SavedTabGroup* group = model_->Get(guid);

  if (!group || !group->is_shared_tab_group()) {
    return;
  }

  std::string client_tag = CreateClientTagForSharedGroup(*group);
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> specifics =
      GetSpecificsForStorageKey(client_tag);
  if (specifics.has_value()) {
    UpdateTabGroupDetailsModel(specifics.value());
  }
}

void SharedTabGroupAccountDataSyncBridge::SavedTabGroupReorderedLocally() {
  if (!is_initialized_ || !change_processor()->IsTrackingMetadata()) {
    // Ignore any changes before the model is successfully initialized.
    return;
  }

  for (const SavedTabGroup* group : model_->GetSharedTabGroupsOnly()) {
    WriteTabGroupDetailToSyncIfPositionChanged(*group);
  }
}

bool SharedTabGroupAccountDataSyncBridge::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_initialized_;
}

bool SharedTabGroupAccountDataSyncBridge::HasSpecificsForTab(
    const SavedTabGroupTab& tab) const {
  const SavedTabGroup* group = model_->Get(tab.saved_group_guid());
  CHECK(group);
  return specifics_.contains(CreateClientTagForSharedTab(*group, tab));
}

std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>
SharedTabGroupAccountDataSyncBridge::GetSpecificsForStorageKey(
    const std::string& storage_key) const {
  return specifics_.contains(storage_key)
             ? std::make_optional<>(specifics_.at(storage_key))
             : std::nullopt;
}

void SharedTabGroupAccountDataSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(base::BindOnce(
      &SharedTabGroupAccountDataSyncBridge::OnReadAllDataAndMetadata,
      weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupAccountDataSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  specifics_.reserve(entries->size());
  for (const syncer::DataTypeStore::Record& r : *entries) {
    sync_pb::SharedTabGroupAccountDataSpecifics specifics;
    if (!specifics.ParseFromString(r.value)) {
      // Ignore invalid entries.
      continue;
    }
    const std::string storage_key = GetClientTagFromSpecifics(specifics);
    if (storage_key != r.id) {
      // GUID is used as a storage key, so it should always match.
      continue;
    }

    specifics_[storage_key] = specifics;
    if (specifics.has_shared_tab_details()) {
      UpdateTabDetailsModel(specifics);
    } else if (specifics.has_shared_tab_group_details()) {
      UpdateTabGroupDetailsModel(specifics);
    }
  }

  is_initialized_ = true;
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void SharedTabGroupAccountDataSyncBridge::OnDataTypeStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    change_processor()->ReportError(*error);
  }
}

void SharedTabGroupAccountDataSyncBridge::UpdateTabDetailsModel(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  // Can only be called with specifics containing TabDetails.
  CHECK(specifics.has_shared_tab_details());

  const std::string storage_key = GetClientTagFromSpecifics(specifics);
  const base::Uuid group_id = base::Uuid::ParseCaseInsensitive(
      specifics.shared_tab_details().shared_tab_group_guid());
  const SavedTabGroup* group = model_->Get(group_id);
  if (!group) {
    storage_keys_for_missing_tabs_.insert(storage_key);
    return;
  }

  const base::Uuid tab_id = base::Uuid::ParseCaseInsensitive(specifics.guid());
  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  if (!tab) {
    storage_keys_for_missing_tabs_.insert(storage_key);
    return;
  }

  model_->UpdateTabLastSeenTime(
      group_id, tab_id,
      DeserializeTime(
          specifics.shared_tab_details().last_seen_timestamp_windows_epoch()),
      TriggerSource::REMOTE);

  storage_keys_for_missing_tabs_.erase(storage_key);
}

void SharedTabGroupAccountDataSyncBridge::UpdateTabGroupDetailsModel(
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics) {
  CHECK(specifics.has_shared_tab_group_details());

  const std::string storage_key = GetClientTagFromSpecifics(specifics);
  const base::Uuid tab_group_id =
      base::Uuid::ParseCaseInsensitive(specifics.guid());
  const SavedTabGroup* group = model_->Get(tab_group_id);
  if (!group) {
    storage_keys_for_missing_tab_groups_.insert(storage_key);
    return;
  }

  // If the group is in the model, update its position based on the specifics.
  std::optional<size_t> position;
  if (specifics.shared_tab_group_details().has_pinned_position()) {
    position = specifics.shared_tab_group_details().pinned_position();
  }
  model_->UpdatePositionForSharedGroupFromSync(tab_group_id, position);

  storage_keys_for_missing_tab_groups_.erase(storage_key);
}

void SharedTabGroupAccountDataSyncBridge::ApplyMissingTabData() {
  // Find previously missing tabs have now been added. Create a copy of
  // the strings from `storage_keys_for_missing_tabs_` so this set can
  // be mutated in `UpdateTabDetailsModel`.
  std::vector<std::string> added_tabs;
  added_tabs.reserve(storage_keys_for_missing_tabs_.size());

  for (const std::string& storage_key : storage_keys_for_missing_tabs_) {
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
        specifics_.at(storage_key);
    if (SharedTabExistsForSpecifics(*model_, specifics)) {
      added_tabs.emplace_back(storage_key);
    }
  }

  for (const std::string& storage_key : added_tabs) {
    UpdateTabDetailsModel(specifics_.at(storage_key));
  }
}

void SharedTabGroupAccountDataSyncBridge::ApplyMissingTabGroupData() {
  // Find previously missing tab groups have now been added. Create a copy of
  // the strings from `storage_keys_for_missing_tab_groups_` so this set can
  // be mutated in `UpdateTabGroupDetailsModel`.
  std::vector<std::string> groups_in_model;
  groups_in_model.reserve(storage_keys_for_missing_tab_groups_.size());

  for (const std::string& storage_key : storage_keys_for_missing_tab_groups_) {
    const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
        specifics_.at(storage_key);
    const base::Uuid tab_group_id =
        base::Uuid::ParseCaseInsensitive(specifics.guid());
    if (model_->Get(tab_group_id)) {
      groups_in_model.emplace_back(storage_key);
    }
  }

  for (const std::string& storage_key : groups_in_model) {
    UpdateTabGroupDetailsModel(specifics_.at(storage_key));
  }
}

void SharedTabGroupAccountDataSyncBridge::WriteEntityToSync(
    std::unique_ptr<syncer::EntityData> entity) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  const sync_pb::SharedTabGroupAccountDataSpecifics& specifics =
      entity->specifics.shared_tab_group_account_data();
  const std::string storage_key = GetStorageKey(*entity);

  specifics_[storage_key] = specifics;
  if (specifics.has_shared_tab_details()) {
    // For tab details, remove them from the missing tabs.
    storage_keys_for_missing_tabs_.erase(storage_key);
  } else if (specifics.has_shared_tab_group_details()) {
    storage_keys_for_missing_tab_groups_.erase(storage_key);
  }

  batch->WriteData(storage_key, specifics.SerializeAsString());

  change_processor()->Put(storage_key, std::move(entity),
                          batch->GetMetadataChangeList());

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(
          &SharedTabGroupAccountDataSyncBridge::OnDataTypeStoreCommit,
          weak_ptr_factory_.GetWeakPtr()));
}

void SharedTabGroupAccountDataSyncBridge::RemoveEntitySpecifics(
    const std::string& storage_key) {
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  // Remove the entity from in-memory cache, storage, and sync.
  specifics_.erase(storage_key);
  storage_keys_for_missing_tabs_.erase(storage_key);
  batch->DeleteData(storage_key);
  change_processor()->Delete(storage_key, syncer::DeletionOrigin::Unspecified(),
                             batch->GetMetadataChangeList());
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(
          &SharedTabGroupAccountDataSyncBridge::OnDataTypeStoreCommit,
          weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<syncer::EntityData>
SharedTabGroupAccountDataSyncBridge::CreateEntityDataFromSharedTabGroup(
    const SavedTabGroupModel& model,
    const SavedTabGroup& tab_group) {
  CHECK(tab_group.is_shared_tab_group());

  // WARNING: all fields need to be set or cleared explicitly.
  // WARNING: if you are adding support for new
  // `SharedTabGroupAccountDataSpecifics` fields, you need to update the
  // following functions accordingly: `TrimSpecifics`.
  sync_pb::SharedTabGroupAccountDataSpecifics specifics =
      change_processor()
          ->GetPossiblyTrimmedRemoteSpecifics(
              CreateClientTagForSharedGroup(tab_group))
          .shared_tab_group_account_data();

  specifics.set_guid(tab_group.saved_guid().AsLowercaseString());
  specifics.set_collaboration_id(tab_group.collaboration_id()->value());
  specifics.set_version(kCurrentSharedTabGroupAccountDataSpecificsProtoVersion);

  sync_pb::SharedTabGroupDetails* tab_group_details =
      specifics.mutable_shared_tab_group_details();
  if (tab_group.position().has_value()) {
    tab_group_details->set_pinned_position(tab_group.position().value());
  }

  return CreateEntityDataFromSpecifics(specifics);
}

std::unique_ptr<syncer::EntityData>
SharedTabGroupAccountDataSyncBridge::CreateEntityDataFromSavedTabGroupTab(
    const SavedTabGroupModel& model,
    const SavedTabGroupTab& tab) {
  const SavedTabGroup* group = model.Get(tab.saved_group_guid());
  CHECK(group);

  const std::optional<CollaborationId>& collaboration_id =
      group->collaboration_id();
  CHECK(collaboration_id.has_value());

  // WARNING: all fields need to be set or cleared explicitly.
  // WARNING: if you are adding support for new
  // `SharedTabGroupAccountDataSpecifics` fields, you need to update the
  // following functions accordingly: `TrimSpecifics`.
  sync_pb::SharedTabGroupAccountDataSpecifics specifics =
      change_processor()
          ->GetPossiblyTrimmedRemoteSpecifics(
              CreateClientTagForSharedTab(*group, tab))
          .shared_tab_group_account_data();

  specifics.set_guid(tab.saved_tab_guid().AsLowercaseString());
  specifics.set_collaboration_id(collaboration_id->value());
  specifics.set_version(kCurrentSharedTabGroupAccountDataSpecificsProtoVersion);

  sync_pb::SharedTabDetails* tab_group_details =
      specifics.mutable_shared_tab_details();
  tab_group_details->set_shared_tab_group_guid(
      group->saved_guid().AsLowercaseString());
  tab_group_details->set_last_seen_timestamp_windows_epoch(
      SerializeTime(tab.last_seen_time().value()));

  return CreateEntityDataFromSpecifics(specifics);
}

void SharedTabGroupAccountDataSyncBridge::MaybeRemoveTabDetailsOnGroupUpdate(
    const SavedTabGroup& group,
    const std::optional<base::Uuid>& tab_guid) {
  if (!tab_guid) {
    return;
  }

  const SavedTabGroupTab* tab = group.GetTab(tab_guid.value());
  if (tab) {
    return;
  }

  // This is an update for a shared tab deletion from local. Remove the
  // corresponding entity from sync.
  const std::string storage_key = CreateClientTagForSharedTab(
      group.collaboration_id().value(), tab_guid.value());
  RemoveEntitySpecifics(storage_key);
}

void SharedTabGroupAccountDataSyncBridge::
    WriteTabGroupDetailToSyncIfPositionChanged(const SavedTabGroup& group) {
  std::string client_tag = CreateClientTagForSharedGroup(group);
  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics> specifics =
      GetSpecificsForStorageKey(client_tag);
  bool has_changed = false;
  if (specifics.has_value()) {
    std::optional<size_t> specifics_pinned_position;
    if (specifics->has_shared_tab_group_details()) {
      if (specifics->shared_tab_group_details().has_pinned_position()) {
        specifics_pinned_position =
            specifics->shared_tab_group_details().pinned_position();
      }
    }
    if (group.position() != specifics_pinned_position) {
      has_changed = true;
    }
  } else {
    has_changed = true;
  }

  if (has_changed) {
    WriteEntityToSync(CreateEntityDataFromSharedTabGroup(*model_, group));
  }
}

}  // namespace tab_groups
