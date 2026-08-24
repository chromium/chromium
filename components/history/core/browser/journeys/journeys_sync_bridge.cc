// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_sync_bridge.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/history/core/browser/journeys/journeys_sync_metadata_database.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/journey_specifics.pb.h"

namespace history::journeys {

JourneysSyncBridge::JourneysSyncBridge(
    JourneysSyncMetadataDatabase* sync_metadata_database,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      sync_metadata_database_(sync_metadata_database) {
  LoadMetadata();
}

JourneysSyncBridge::~JourneysSyncBridge() = default;

std::optional<syncer::ModelError> JourneysSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is a read-only data type, meaning that no data originates locally,
  // hence there is nothing to merge.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

std::optional<syncer::ModelError>
JourneysSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/526686844): Implement me.
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> JourneysSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // JOURNEY is a read-only client data type. Committing local journeys back
  // to the server is not supported for now.
  NOTREACHED();
}

std::unique_ptr<syncer::DataBatch>
JourneysSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/526686844): Implement me.
  return std::make_unique<syncer::MutableDataBatch>();
}

// The global immutable sync identity, used to identify entities across devices.
// For Journeys, each journey generated on the server is assigned a unique GUID
// (journey_id). Returning journey_id ensures that incoming updates for the same
// Journey on different devices or sync cycles map to the exact same entity.
std::string JourneysSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.journey().journey_id();
}

// The local database primary key, representing the key under which this entity
// is stored in the local database. When the sync processor receives
// incoming updates from the server, it uses this function to route the entity
// to the corresponding record in the local database.
std::string JourneysSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTag(entity_data);
}

// Invoked before any received update from the server is processed or passed to
// ApplyIncrementalSyncChanges.
bool JourneysSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.has_journey() &&
         !entity_data.specifics.journey().journey_id().empty();
}

sync_pb::EntitySpecifics
JourneysSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // JOURNEY is currently a read-only client data type, but trimmed specifics
  // are returned in case local commits are supported in the future.
  // LINT.IfChange(TrimAllSupportedFieldsFromRemoteSpecifics)
  sync_pb::JourneySpecifics trimmed_specifics = entity_specifics.journey();
  trimmed_specifics.clear_journey_id();
  trimmed_specifics.clear_title();
  trimmed_specifics.clear_emoji();
  trimmed_specifics.clear_overview();
  trimmed_specifics.clear_short_overview();
  trimmed_specifics.clear_creation_time_windows_epoch_micros();
  trimmed_specifics.clear_history_entries();
  trimmed_specifics.clear_continuation_queries();
  // LINT.ThenChange(//components/sync/protocol/journey_specifics.proto:JourneySpecifics)

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  if (trimmed_specifics.ByteSizeLong() > 0) {
    *trimmed_entity_specifics.mutable_journey() = std::move(trimmed_specifics);
  }
  return trimmed_entity_specifics;
}

// Called when all sync metadata must be wiped from local storage, i.e. when
// the user signs out of their Google Account, toggles off the specific
// data type in sync settings, or sync encounters an unrecoverable state.
void JourneysSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sync_metadata_database_) {
    sync_metadata_database_->ClearAllEntityMetadata();
    sync_metadata_database_->ClearDataTypeState(syncer::JOURNEY);
  }
  // TODO(crbug.com/526686844): Also wipe journey data.
}

void JourneysSyncBridge::LoadMetadata() {
  if (!sync_metadata_database_) {
    return;
  }
  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!sync_metadata_database_->GetAllSyncMetadata(batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, syncer::ModelError::Type::kJourneysFailedToLoadMetadata});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

// Creates a SyncMetadataStoreChangeList connected to
// JourneysSyncMetadataDatabase.
//
// This is called by the DataTypeLocalChangeProcessor when receiving remote
// server updates (MergeFullSyncData, ApplyIncrementalSyncChanges) or tearing
// down sync (ApplyDisableSyncChanges). It provides the change list used to
// write DataTypeState and EntityMetadata directly into the History SQLite
// database.
std::unique_ptr<syncer::MetadataChangeList>
JourneysSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      sync_metadata_database_, syncer::JOURNEY,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

void JourneysSyncBridge::OnDatabaseError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_metadata_database_ = nullptr;
  change_processor()->ReportError(
      {FROM_HERE, syncer::ModelError::Type::kJourneysDatabaseError});
}

}  // namespace history::journeys
