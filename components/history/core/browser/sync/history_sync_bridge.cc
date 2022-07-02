// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_sync_bridge.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/sync/history_sync_metadata_database.h"
#include "components/sync/base/page_transition_conversion.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"

namespace history {

namespace {

constexpr base::TimeDelta kMaxWriteToTheFuture = base::Days(2);

enum class SyncHistoryDatabaseError {
  // TODO(crbug.com/1318028): Consider introducing separate buckets for
  // MergeSyncData vs ApplySyncChanges.
  kApplySyncChangesAddSyncedVisit = 1,
  kApplySyncChangesWriteMetadata = 2,
  kOnDatabaseError = 3,
  kLoadMetadata = 4,
};

void RecordDatabaseError(SyncHistoryDatabaseError error) {
  DLOG(ERROR) << "SyncHistoryBridge database error: "
              << static_cast<int>(error);
  // TODO(crbug.com/1318028): Record UMA histogram, and add "do not modify"
  // comment to the enum.
}

// Creates a VisitRow out of a single redirect entry within the `specifics`.
// The `visit_id` and `url_id` will be unset; the HistoryBackend assigns those.
VisitRow MakeVisitRow(const sync_pb::HistorySpecifics& specifics,
                      int redirect_index) {
  DCHECK_GE(redirect_index, 0);
  DCHECK_LT(redirect_index, specifics.redirect_entries_size());

  VisitRow row;
  // Required fields: `visit_time` and `originator_cache_guid`.
  DCHECK_NE(specifics.visit_time_windows_epoch_micros(), 0);
  DCHECK(!specifics.originator_cache_guid().empty());
  row.visit_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.visit_time_windows_epoch_micros()));
  row.originator_cache_guid = specifics.originator_cache_guid();

  // The `originator_visit_id` should always exist for visits coming from modern
  // clients, but it may be missing in visits from legacy clients (i.e. clients
  // committing history data via the SESSIONS data type).
  row.originator_visit_id =
      specifics.redirect_entries(redirect_index).originator_visit_id();

  // Reconstruct the page transition - first get the core type.
  int page_transition = syncer::FromSyncPageTransition(
      specifics.page_transition().core_transition());
  // Then add qualifiers (stored in separate proto fields).
  if (specifics.page_transition().blocked()) {
    page_transition |= ui::PAGE_TRANSITION_BLOCKED;
  }
  if (specifics.page_transition().forward_back()) {
    page_transition |= ui::PAGE_TRANSITION_FORWARD_BACK;
  }
  if (specifics.page_transition().from_address_bar()) {
    page_transition |= ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  }
  if (specifics.page_transition().home_page()) {
    page_transition |= ui::PAGE_TRANSITION_HOME_PAGE;
  }
  // Then add redirect markers as appropriate - first chain start/end markers.
  if (redirect_index == 0) {
    page_transition |= ui::PAGE_TRANSITION_CHAIN_START;
  }
  // No "else" - a visit can be both the start and end of a chain!
  if (redirect_index == specifics.redirect_entries_size() - 1) {
    page_transition |= ui::PAGE_TRANSITION_CHAIN_END;
  }
  // Finally, add the redirect type (if any).
  if (specifics.redirect_entries(redirect_index).has_redirect_type()) {
    switch (specifics.redirect_entries(redirect_index).redirect_type()) {
      case sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT:
        page_transition |= ui::PAGE_TRANSITION_CLIENT_REDIRECT;
        break;
      case sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT:
        page_transition |= ui::PAGE_TRANSITION_SERVER_REDIRECT;
        break;
    }
  }
  row.transition = ui::PageTransitionFromInt(page_transition);

  // The first visit in a chain stores the referring/opener visit (if any).
  if (redirect_index == 0) {
    row.originator_referring_visit = specifics.originator_referring_visit_id();
    row.originator_opener_visit = specifics.originator_opener_visit_id();
  }
  // The last visit in a chain stores the visit duration (earlier visits, i.e.
  // redirects, are not considered to have a duration).
  if (redirect_index == specifics.redirect_entries_size() - 1) {
    row.visit_duration = base::Microseconds(specifics.visit_duration_micros());
  }

  return row;
}

enum class SpecificsError {
  kMissingRequiredFields = 1,
  kTooOld = 2,
  kTooNew = 3,
};

// Checks the given `specifics` for validity, i.e. whether it passes some basic
// validation checks, and returns the appropriate error if it doesn't.
absl::optional<SpecificsError> GetSpecificsError(
    const sync_pb::HistorySpecifics& specifics,
    const HistoryBackendForSync* history_backend) {
  // Check for required fields: visit_time and originator_cache_guid must not be
  // empty, and there must be at least one entry in the redirects list.
  if (specifics.visit_time_windows_epoch_micros() == 0 ||
      specifics.originator_cache_guid().empty() ||
      specifics.redirect_entries_size() == 0) {
    return SpecificsError::kMissingRequiredFields;
  }

  base::Time visit_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.visit_time_windows_epoch_micros()));

  // Already-expired visits are not valid. (They wouldn't really cause any harm,
  // but the history backend would just immediately expire them.)
  if (history_backend->IsExpiredVisitTime(visit_time)) {
    return SpecificsError::kTooOld;
  }

  // Visits that are too far in the future are not valid.
  if (visit_time > base::Time::Now() + kMaxWriteToTheFuture) {
    return SpecificsError::kTooNew;
  }

  return {};
}

void RecordSpecificsError(SpecificsError validity) {
  // TODO(crbug.com/1318028): Record UMA histogram, and add "do not modify"
  // comment to the enum.
}

}  // namespace

HistorySyncBridge::HistorySyncBridge(
    HistoryBackendForSync* history_backend,
    HistorySyncMetadataDatabase* sync_metadata_database,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)),
      history_backend_(history_backend),
      sync_metadata_database_(sync_metadata_database) {
  DCHECK(history_backend_);
  DCHECK(sync_metadata_database_);
  // Note that `sync_metadata_database_` can become null later, in case of
  // database errors.

  LoadMetadata();
}

HistorySyncBridge::~HistorySyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
HistorySyncBridge::CreateMetadataChangeList() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      sync_metadata_database_, syncer::HISTORY);
}

absl::optional<syncer::ModelError> HistorySyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // Note: History is not synced retroactively - only visits created *after*
  // turning Sync on get synced. So there's nothing to upload here. Just apply
  // the incoming changes to the local history DB.
  return ApplySyncChanges(std::move(metadata_change_list),
                          std::move(entity_data));
}

absl::optional<syncer::ModelError> HistorySyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(!processing_syncer_changes_);
  // Set flag to stop accepting history change notifications from backend.
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  for (const std::unique_ptr<syncer::EntityChange>& entity_change :
       entity_changes) {
    DCHECK(entity_change->data().specifics.has_history());
    const sync_pb::HistorySpecifics& specifics =
        entity_change->data().specifics.history();

    // Check validity requirements.
    absl::optional<SpecificsError> specifics_error =
        GetSpecificsError(specifics, history_backend_);
    if (specifics_error.has_value()) {
      DLOG(ERROR) << "Skipping invalid visit, reason "
                  << static_cast<int>(*specifics_error);
      RecordSpecificsError(*specifics_error);
      // If this was a newly-added visit, immediately untrack it again.
      if (entity_change->type() == syncer::EntityChange::ACTION_ADD) {
        change_processor()->UntrackEntityForClientTagHash(
            entity_change->data().client_tag_hash);
      }
      continue;
    }

    switch (entity_change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        // First try updating an existing row. In addition to actual updates,
        // this can also happen during initial merge (if Sync was enabled before
        // and this entity was already downloaded back then).
        // TODO(crbug.com/1329131): ...or if the visit was untracked.
        if (UpdateEntityInBackend(specifics)) {
          // Updating worked - there was a matching visit in the DB already.
          // This happens during initial merge, or when an existing visit got
          // untracked. Nothing further to be done here.
        } else {
          // Updating didn't work, so actually add the data instead.
          if (!AddEntityInBackend(specifics)) {
            // Something went wrong - stop tracking the entity.
            RecordDatabaseError(
                SyncHistoryDatabaseError::kApplySyncChangesAddSyncedVisit);
            change_processor()->UntrackEntityForClientTagHash(
                entity_change->data().client_tag_hash);
            break;
          }
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE:
        // Deletes are not supported - they're handled via
        // HISTORY_DELETE_DIRECTIVE instead.
        DLOG(ERROR) << "Received unexpected deletion for HISTORY";
        break;
    }
  }

  absl::optional<syncer::ModelError> metadata_error =
      static_cast<syncer::SyncMetadataStoreChangeList*>(
          metadata_change_list.get())
          ->TakeError();
  if (metadata_error) {
    RecordDatabaseError(
        SyncHistoryDatabaseError::kApplySyncChangesWriteMetadata);
  }
  return metadata_error;
}

void HistorySyncBridge::GetData(StorageKeyList storage_keys,
                                DataCallback callback) {
  NOTIMPLEMENTED();
}

void HistorySyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string HistorySyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  return GetStorageKey(entity_data);
}

std::string HistorySyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(entity_data.specifics.has_history())
      << "EntityData does not have history specifics.";

  const sync_pb::HistorySpecifics& history = entity_data.specifics.history();
  return HistorySyncMetadataDatabase::StorageKeyFromMicrosSinceWindowsEpoch(
      history.visit_time_windows_epoch_micros());
}

void HistorySyncBridge::OnURLVisited(HistoryBackend* history_backend,
                                     ui::PageTransition transition,
                                     const URLRow& row,
                                     base::Time visit_time) {
  NOTIMPLEMENTED();
}

void HistorySyncBridge::OnURLsModified(HistoryBackend* history_backend,
                                       const URLRows& changed_urls,
                                       bool is_from_expiration) {
  NOTIMPLEMENTED();
}

void HistorySyncBridge::OnURLsDeleted(HistoryBackend* history_backend,
                                      bool all_history,
                                      bool expired,
                                      const URLRows& deleted_rows,
                                      const std::set<GURL>& favicon_urls) {
  NOTIMPLEMENTED();
}

void HistorySyncBridge::OnDatabaseError() {
  sync_metadata_database_ = nullptr;
  RecordDatabaseError(SyncHistoryDatabaseError::kOnDatabaseError);
  change_processor()->ReportError(
      {FROM_HERE, "HistoryDatabase encountered error"});
}

void HistorySyncBridge::LoadMetadata() {
  // `sync_metadata_database_` can become null in case of database errors, but
  // this is the very first usage of it, so here it can't be null yet.
  DCHECK(sync_metadata_database_);

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!sync_metadata_database_->GetAllSyncMetadata(batch.get())) {
    RecordDatabaseError(SyncHistoryDatabaseError::kLoadMetadata);
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading metadata from HistorySyncMetadataDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

bool HistorySyncBridge::AddEntityInBackend(
    const sync_pb::HistorySpecifics& specifics) {
  // Add all the visits in the redirect chain. Populate the `referring_visit`
  // IDs along the way.
  VisitID referring_visit_id = 0;
  for (int i = 0; i < specifics.redirect_entries_size(); i++) {
    VisitRow visit_row = MakeVisitRow(specifics, i);
    visit_row.referring_visit = referring_visit_id;
    VisitID added_visit_id = history_backend_->AddSyncedVisit(
        GURL(specifics.redirect_entries(i).url()),
        base::UTF8ToUTF16(specifics.redirect_entries(i).title()),
        specifics.redirect_entries(i).hidden(), visit_row);
    if (added_visit_id == 0) {
      // Visit failed to be added to the DB - unclear if/how this can happen.
      return false;
    }
    referring_visit_id = added_visit_id;
  }
  // TODO(crbug.com/1335055): Remap the originator_referring_visit and
  // originator_opener_visit fields to local visit IDs.

  return true;
}

bool HistorySyncBridge::UpdateEntityInBackend(
    const sync_pb::HistorySpecifics& specifics) {
  // Only try updating the final visit in a chain - earlier visits (i.e.
  // redirects) can't get updated anyway.
  // TODO(crbug.com/1318028): Verify whether only updating the chain end
  // is indeed sufficient.
  VisitRow final_visit_row =
      MakeVisitRow(specifics, specifics.redirect_entries_size() - 1);
  if (!history_backend_->UpdateSyncedVisit(final_visit_row)) {
    return false;
  }

  // TODO(crbug.com/1318028): Handle updates to the URL-related fields
  // (notably the title - other fields probably can't change).
  return true;
}

}  // namespace history
