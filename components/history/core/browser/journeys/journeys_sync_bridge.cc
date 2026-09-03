// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_sync_bridge.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/history/core/browser/journeys/history_backend_for_journeys_sync.h"
#include "components/history/core/browser/journeys/journeys_sync_metadata_database.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/journey_specifics.pb.h"

namespace history::journeys {

namespace {

// LINT.IfChange(JourneySpecificsConversions)
JourneyRow JourneyRowFromSpecifics(const sync_pb::JourneySpecifics& specifics) {
  JourneyRow row;
  row.journey_id = specifics.journey_id();
  row.title = specifics.title();
  if (specifics.has_emoji()) {
    row.emoji = specifics.emoji();
  }
  if (specifics.has_overview()) {
    row.overview = specifics.overview();
  }
  if (specifics.has_short_overview()) {
    row.short_overview = specifics.short_overview();
  }
  row.creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.creation_time_windows_epoch_micros()));

  for (const auto& entry : specifics.history_entries()) {
    row.history_entries.emplace_back(base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(entry.visit_timestamp_windows_epoch_micros())));
  }

  for (const auto& query : specifics.continuation_queries()) {
    row.continuation_queries.emplace_back(query.title(), query.prompt());
  }

  return row;
}

sync_pb::JourneySpecifics SpecificsFromJourneyRow(const JourneyRow& row) {
  sync_pb::JourneySpecifics specifics;
  specifics.set_journey_id(row.journey_id);
  specifics.set_title(row.title);
  if (row.emoji.has_value()) {
    specifics.set_emoji(*row.emoji);
  }
  if (row.overview.has_value()) {
    specifics.set_overview(*row.overview);
  }
  if (row.short_overview.has_value()) {
    specifics.set_short_overview(*row.short_overview);
  }
  specifics.set_creation_time_windows_epoch_micros(
      row.creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  for (const auto& entry : row.history_entries) {
    specifics.add_history_entries()->set_visit_timestamp_windows_epoch_micros(
        entry.visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  }

  for (const auto& query : row.continuation_queries) {
    auto* q = specifics.add_continuation_queries();
    q->set_title(query.title);
    q->set_prompt(query.prompt);
  }

  return specifics;
}
// LINT.ThenChange(//components/sync/protocol/journey_specifics.proto:JourneySpecifics)

}  // namespace

JourneysSyncBridge::JourneysSyncBridge(
    HistoryBackendForJourneysSync* backend,
    JourneysSyncMetadataDatabase* sync_metadata_database,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      backend_(CHECK_DEREF(backend)),
      sync_metadata_database_(sync_metadata_database) {
  history_backend_observation_.Observe(&backend_.get());
  LoadMetadata();
}

JourneysSyncBridge::~JourneysSyncBridge() = default;

std::optional<syncer::ModelError> JourneysSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is a read-only data type, meaning that no data originates locally,
  // hence there is nothing to merge.
  // TODO(crbug.com/526686844): Consider whether an explicit startup cleanup
  // mechanism is needed if data clearing on signout is ever interrupted.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

// Note: `metadata_change_list` does not need to be committed here because
// JourneysSyncMetadataDatabase writes updates directly to SQLite via
// SyncMetadataStoreChangeList while the processor processes updates.
std::optional<syncer::ModelError>
JourneysSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<JourneyRow> journeys_to_add_or_update;
  std::vector<std::string> journey_ids_to_delete;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        DCHECK(change->data().specifics.has_journey());
        journeys_to_add_or_update.push_back(
            JourneyRowFromSpecifics(change->data().specifics.journey()));
        break;
      }
      case syncer::EntityChange::ACTION_DELETE: {
        journey_ids_to_delete.push_back(change->storage_key());
        break;
      }
    }
  }

  // The processor squashes multiple changes to the same entity within a batch,
  // so deletions and additions/updates will not conflict.
  if (!journey_ids_to_delete.empty()) {
    if (!backend_->DeleteJourneys(journey_ids_to_delete)) {
      return syncer::ModelError(
          FROM_HERE, syncer::ModelError::Type::kJourneysDatabaseError);
    }
  }

  if (!journeys_to_add_or_update.empty()) {
    if (!backend_->AddOrUpdateJourneys(journeys_to_add_or_update)) {
      return syncer::ModelError(
          FROM_HERE, syncer::ModelError::Type::kJourneysDatabaseError);
    }
  }

  return change_processor()->GetError();
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
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& journey_row : backend_->GetAllJourneys()) {
    auto entity_data = std::make_unique<syncer::EntityData>();
    entity_data->name = journey_row.journey_id;
    *entity_data->specifics.mutable_journey() =
        SpecificsFromJourneyRow(journey_row);
    batch->Put(journey_row.journey_id, std::move(entity_data));
  }
  return batch;
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
  backend_->DeleteAllJourneys();
}

void JourneysSyncBridge::OnURLVisited(HistoryBackend* history_backend,
                                      const URLRow& url_row,
                                      const VisitRow& visit_row) {}

void JourneysSyncBridge::OnURLsModified(HistoryBackend* history_backend,
                                        const URLRows& changed_urls,
                                        bool is_from_expiration) {}

void JourneysSyncBridge::OnHistoryDeletions(
    HistoryBackend* history_backend,
    bool all_history,
    bool expired,
    const URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Individual deletions do not require clearing all sync metadata. But if all
  // history is cleared, there are no individual notifications, so handle that
  // case here by untracking all entities and clearing their metadata.
  if (!all_history) {
    return;
  }
  UntrackAndClearMetadataForAllEntities();
}

void JourneysSyncBridge::OnVisitUpdated(const VisitRow& visit,
                                        VisitUpdateReason reason) {}

void JourneysSyncBridge::OnVisitDeleted(const VisitRow& visit) {}

void JourneysSyncBridge::UntrackAndClearMetadataForAllEntities() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sync_metadata_database_) {
    sync_metadata_database_->ClearAllEntityMetadata();
  }
  for (const std::string& storage_key :
       change_processor()->GetAllTrackedStorageKeys()) {
    change_processor()->UntrackEntityForStorageKey(storage_key);
  }
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
