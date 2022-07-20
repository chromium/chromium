// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_sync_bridge.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/history/core/browser/sync/history_sync_metadata_database.h"
#include "components/history/core/browser/sync/visit_id_remapper.h"
#include "components/sync/base/page_transition_conversion.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/history_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"

namespace history {

namespace {

constexpr base::TimeDelta kMaxWriteToTheFuture = base::Days(2);

std::string GetStorageKeyFromVisitRow(const VisitRow& row) {
  DCHECK(!row.visit_time.is_null());
  return HistorySyncMetadataDatabase::StorageKeyFromVisitTime(row.visit_time);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SyncHistoryDatabaseError {
  kApplySyncChangesAddSyncedVisit = 0,
  kApplySyncChangesWriteMetadata = 1,
  kOnDatabaseError = 2,
  kLoadMetadata = 3,
  kOnURLVisitedGetVisit = 4,
  kOnURLsDeletedReadMetadata = 5,
  kOnVisitUpdatedGetURL = 6,
  kGetAllDataReadMetadata = 7,
  kMaxValue = kGetAllDataReadMetadata
};

void RecordDatabaseError(SyncHistoryDatabaseError error) {
  DLOG(ERROR) << "SyncHistoryBridge database error: "
              << static_cast<int>(error);
  base::UmaHistogramEnumeration("Sync.History.DatabaseError", error);
}

base::Time GetVisitTime(const sync_pb::HistorySpecifics& specifics) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.visit_time_windows_epoch_micros()));
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
  row.visit_time = GetVisitTime(specifics);
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

std::unique_ptr<syncer::EntityData> MakeEntityData(
    const std::string& local_cache_guid,
    const std::vector<URLRow>& redirect_urls,
    const std::vector<VisitRow>& redirect_visits) {
  DCHECK(!local_cache_guid.empty());
  DCHECK(!redirect_urls.empty());
  DCHECK_EQ(redirect_urls.size(), redirect_visits.size());

  auto entity_data = std::make_unique<syncer::EntityData>();
  sync_pb::HistorySpecifics* history = entity_data->specifics.mutable_history();

  // The first and last visit in the redirect chain are special: The first is
  // where the user intended to go (via typing the URL, clicking on a link, etc)
  // and the last one is where they actually ended up.
  const VisitRow& first_visit = redirect_visits.front();
  const VisitRow& last_visit = redirect_visits.back();

  // Take the visit time and the originator client ID from the last visit,
  // though they should be the same across all visits in the chain anyway.
  history->set_visit_time_windows_epoch_micros(
      last_visit.visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  const bool is_local_entity = last_visit.originator_cache_guid.empty();
  history->set_originator_cache_guid(
      is_local_entity ? local_cache_guid : last_visit.originator_cache_guid);

  for (size_t i = 0; i < redirect_urls.size(); i++) {
    const URLRow& url = redirect_urls[i];
    const VisitRow& visit = redirect_visits[i];
    auto* redirect_entry = history->add_redirect_entries();
    redirect_entry->set_originator_visit_id(
        is_local_entity ? visit.visit_id : visit.originator_visit_id);
    redirect_entry->set_url(url.url().spec());
    redirect_entry->set_title(base::UTF16ToUTF8(url.title()));
    redirect_entry->set_hidden(url.hidden());

    if (ui::PageTransitionIsRedirect(visit.transition)) {
      if (visit.transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
        redirect_entry->set_redirect_type(
            sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT);
      } else {
        // Since we checked ui::PageTransitionIsRedirect(), either the client or
        // the server redirect flag must be set.
        DCHECK(visit.transition & ui::PAGE_TRANSITION_SERVER_REDIRECT);
        redirect_entry->set_redirect_type(
            sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT);
      }
    }
  }

  // The transition should be the same across the whole redirect chain, apart
  // from redirect-related qualifiers. Take the transition from the first visit.
  history->mutable_page_transition()->set_core_transition(
      syncer::ToSyncPageTransition(first_visit.transition));
  history->mutable_page_transition()->set_blocked(
      (first_visit.transition & ui::PAGE_TRANSITION_BLOCKED) != 0);
  history->mutable_page_transition()->set_forward_back(
      (first_visit.transition & ui::PAGE_TRANSITION_FORWARD_BACK) != 0);
  history->mutable_page_transition()->set_from_address_bar(
      (first_visit.transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0);
  history->mutable_page_transition()->set_home_page(
      (first_visit.transition & ui::PAGE_TRANSITION_HOME_PAGE) != 0);

  // Referring visit and opener visit are taken from the *first* visit in the
  // chain, since they only make sense for that one.
  history->set_originator_referring_visit_id(first_visit.referring_visit);
  history->set_originator_opener_visit_id(first_visit.opener_visit);

  // The final visit is the one where the user actually ended up, so it's the
  // only one that can have a (non-zero) visit duration.
  history->set_visit_duration_micros(
      last_visit.visit_duration.InMicroseconds());

  // The entity name is used for debugging purposes; choose something that's a
  // decent tradeoff between "unique" and "readable".
  entity_data->name =
      base::StringPrintf("%s-%s", history->originator_cache_guid().c_str(),
                         redirect_urls.back().url().spec().c_str());

  return entity_data;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SpecificsError {
  kMissingRequiredFields = 0,
  kTooOld = 1,
  kTooNew = 2,
  kMaxValue = kTooNew
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

  base::Time visit_time = GetVisitTime(specifics);

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

void RecordSpecificsError(SpecificsError error) {
  base::UmaHistogramEnumeration("Sync.History.IncomingSpecificsError", error);
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

  history_backend_observation_.Observe(history_backend_.get());
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

  VisitIDRemapper id_remapper(history_backend_);

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
        if (UpdateEntityInBackend(&id_remapper, specifics)) {
          // Updating worked - there was a matching visit in the DB already.
          // This happens during initial merge, or when an existing visit got
          // untracked. Nothing further to be done here.
        } else {
          // Updating didn't work, so actually add the data instead.
          if (!AddEntityInBackend(&id_remapper, specifics)) {
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

  id_remapper.RemapIDs();

  absl::optional<syncer::ModelError> metadata_error =
      static_cast<syncer::SyncMetadataStoreChangeList*>(
          metadata_change_list.get())
          ->TakeError();
  if (metadata_error) {
    RecordDatabaseError(
        SyncHistoryDatabaseError::kApplySyncChangesWriteMetadata);
  }

  // ApplySyncChanges() gets called both for incoming remote changes (i.e. for
  // GetUpdates) and after a successful Commit. In either case, there's now
  // likely some local metadata that's not needed anymore, so go and clean that
  // up.
  UntrackAndClearMetadataForSyncedEntities();

  return metadata_error;
}

void HistorySyncBridge::GetData(StorageKeyList storage_keys,
                                DataCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    base::Time visit_time =
        HistorySyncMetadataDatabase::StorageKeyToVisitTime(key);
    VisitRow final_visit;
    if (!history_backend_->GetLastVisitByTime(visit_time, &final_visit)) {
      continue;
    }

    // Query the redirect chain that ended in this visit.
    std::vector<VisitRow> redirect_visits =
        history_backend_->GetRedirectChain(final_visit);
    DCHECK(!redirect_visits.empty());
    DCHECK_EQ(redirect_visits.back().visit_id, final_visit.visit_id);

    // Query the corresponding URLs.
    std::vector<URLRow> redirect_urls = QueryURLsForVisits(redirect_visits);

    std::unique_ptr<syncer::EntityData> entity_data =
        MakeEntityData(GetLocalCacheGuid(), redirect_urls, redirect_visits);

    batch->Put(key, std::move(entity_data));
  }

  std::move(callback).Run(std::move(batch));
}

void HistorySyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
  if (!sync_metadata_database_->GetAllSyncMetadata(metadata_batch.get())) {
    RecordDatabaseError(SyncHistoryDatabaseError::kGetAllDataReadMetadata);
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading metadata from HistorySyncMetadataDatabase."});
  }
  StorageKeyList storage_keys;
  for (const auto& [storage_key, metadata] : metadata_batch->GetAllMetadata()) {
    storage_keys.push_back(storage_key);
  }
  GetData(std::move(storage_keys), std::move(callback));
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
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK_GE(row.typed_count(), 0);

  if (processing_syncer_changes_) {
    return;  // These are changes originating from us, ignore.
  }

  if (!change_processor()->IsTrackingMetadata()) {
    return;  // Sync processor not yet ready, don't sync.
  }

  std::vector<VisitRow> visits;
  if (!history_backend_->GetMostRecentVisitsForURL(row.id(), /*max_visits=*/1,
                                                   &visits)) {
    RecordDatabaseError(SyncHistoryDatabaseError::kOnURLVisitedGetVisit);
    return;
  }
  if (visits.size() != 1) {
    RecordDatabaseError(SyncHistoryDatabaseError::kOnURLVisitedGetVisit);
    return;
  }
  const VisitRow& visit_row = visits[0];

  // If this visit is not the end of a redirect chain, ignore it. Note that
  // visits that are not part of a redirect chain are considered to be both
  // start and end of a chain, so these are *not* ignored here.
  if (!(visit_row.transition & ui::PAGE_TRANSITION_CHAIN_END)) {
    return;
  }

  // Query the redirect chain that ended in this visit.
  std::vector<VisitRow> redirect_visits =
      history_backend_->GetRedirectChain(visit_row);
  DCHECK(!redirect_visits.empty());
  DCHECK_EQ(redirect_visits.back().visit_id, visit_row.visit_id);

  // Query the corresponding URLs.
  std::vector<URLRow> redirect_urls = QueryURLsForVisits(redirect_visits);

  std::unique_ptr<syncer::EntityData> entity_data =
      MakeEntityData(GetLocalCacheGuid(), redirect_urls, redirect_visits);

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  change_processor()->Put(GetStorageKeyFromVisitRow(visit_row),
                          std::move(entity_data), metadata_change_list.get());
}

void HistorySyncBridge::OnURLsModified(HistoryBackend* history_backend,
                                       const URLRows& changed_urls,
                                       bool is_from_expiration) {
  // Not interested: This class is watching visits rather than URLs, so
  // modifications are handled in OnVisitUpdated().
  // TODO(crbug.com/1318028): The title *can* get updated without a new visit,
  // so watch for and commit such changes. Basically:
  // - Get most recent visit for the URL.
  // - If it's a local visit, and is tracked (and, maybe, is the end of a
  //   redirect chain):
  // - Build the specifics and Put() it.
}

void HistorySyncBridge::OnURLsDeleted(HistoryBackend* history_backend,
                                      bool all_history,
                                      bool expired,
                                      const URLRows& deleted_rows,
                                      const std::set<GURL>& favicon_urls) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(sync_metadata_database_);

  if (processing_syncer_changes_) {
    return;  // These are changes originating from us, ignore.
  }

  if (!change_processor()->IsTrackingMetadata()) {
    return;  // Sync processor not yet ready, don't sync.
  }

  // If individual URLs get deleted, we're notified about their removed visits
  // via OnVisitDeleted(), so there's nothing to be done here. But if all
  // history is cleared, there are no individual notifications, so handle that
  // case here.
  if (!all_history) {
    return;
  }

  // No need to send any actual deletions: A HistoryDeleteDirective will take
  // care of that. Just untrack all entities and clear their metadata.
  auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
  if (!sync_metadata_database_->GetAllSyncMetadata(metadata_batch.get())) {
    RecordDatabaseError(SyncHistoryDatabaseError::kOnURLsDeletedReadMetadata);
    change_processor()->ReportError(
        {FROM_HERE,
         "Failed reading metadata from HistorySyncMetadataDatabase."});
    return;
  }
  for (const auto& [storage_key, metadata] : metadata_batch->GetAllMetadata()) {
    sync_metadata_database_->ClearSyncMetadata(syncer::HISTORY, storage_key);
    change_processor()->UntrackEntityForStorageKey(storage_key);
  }
}

void HistorySyncBridge::OnVisitUpdated(const VisitRow& visit_row) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(sync_metadata_database_);

  if (processing_syncer_changes_) {
    return;  // These are changes originating from us, ignore.
  }

  if (!change_processor()->IsTrackingMetadata()) {
    return;  // Sync processor not yet ready, don't sync.
  }

  // If this visit is not the end of a redirect chain, ignore it. Note that
  // visits that are not part of a redirect chain are considered to be both
  // start and end of a chain, so these are *not* ignored here.
  if (!(visit_row.transition & ui::PAGE_TRANSITION_CHAIN_END)) {
    return;
  }

  // Query the redirect chain that ended in this visit.
  std::vector<VisitRow> redirect_visits =
      history_backend_->GetRedirectChain(visit_row);
  DCHECK(!redirect_visits.empty());
  DCHECK_EQ(redirect_visits.back().visit_id, visit_row.visit_id);

  // Query the corresponding URLs.
  std::vector<URLRow> redirect_urls = QueryURLsForVisits(redirect_visits);

  std::unique_ptr<syncer::EntityData> entity_data =
      MakeEntityData(GetLocalCacheGuid(), redirect_urls, redirect_visits);

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  change_processor()->Put(GetStorageKeyFromVisitRow(visit_row),
                          std::move(entity_data), metadata_change_list.get());
}

void HistorySyncBridge::OnVisitDeleted(const VisitRow& visit_row) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(sync_metadata_database_);

  if (processing_syncer_changes_) {
    return;  // These are changes originating from us, ignore.
  }

  if (!change_processor()->IsTrackingMetadata()) {
    return;  // Sync processor not yet ready, don't sync.
  }

  // No need to send an actual deletion: Either this was an expiry, in which
  // no deletion should be sent, or if it's an actual deletion, then a
  // HistoryDeleteDirective will take care of that. Just untrack the entity and
  // delete its metadata.
  std::string storage_key = GetStorageKeyFromVisitRow(visit_row);
  sync_metadata_database_->ClearSyncMetadata(syncer::HISTORY, storage_key);
  change_processor()->UntrackEntityForStorageKey(storage_key);
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
    VisitIDRemapper* id_remapper,
    const sync_pb::HistorySpecifics& specifics) {
  // Add all the visits in the redirect chain.
  for (int i = 0; i < specifics.redirect_entries_size(); i++) {
    VisitRow visit_row = MakeVisitRow(specifics, i);
    VisitID added_visit_id = history_backend_->AddSyncedVisit(
        GURL(specifics.redirect_entries(i).url()),
        base::UTF8ToUTF16(specifics.redirect_entries(i).title()),
        specifics.redirect_entries(i).hidden(), visit_row);
    if (added_visit_id == 0) {
      // Visit failed to be added to the DB - unclear if/how this can happen.
      return false;
    }
    id_remapper->RegisterVisit(added_visit_id, visit_row.originator_cache_guid,
                               visit_row.originator_visit_id,
                               visit_row.originator_referring_visit,
                               visit_row.originator_opener_visit);
  }

  return true;
}

bool HistorySyncBridge::UpdateEntityInBackend(
    VisitIDRemapper* id_remapper,
    const sync_pb::HistorySpecifics& specifics) {
  // Only try updating the final visit in a chain - earlier visits (i.e.
  // redirects) can't get updated anyway.
  // TODO(crbug.com/1318028): Verify whether only updating the chain end
  // is indeed sufficient.
  VisitRow final_visit_row =
      MakeVisitRow(specifics, specifics.redirect_entries_size() - 1);
  VisitID updated_visit_id =
      history_backend_->UpdateSyncedVisit(final_visit_row);
  if (updated_visit_id == 0) {
    return false;
  }

  id_remapper->RegisterVisit(updated_visit_id,
                             final_visit_row.originator_cache_guid,
                             final_visit_row.originator_visit_id,
                             final_visit_row.originator_referring_visit,
                             final_visit_row.originator_opener_visit);

  // TODO(crbug.com/1318028): Handle updates to the URL-related fields
  // (notably the title - other fields probably can't change).
  return true;
}

void HistorySyncBridge::UntrackAndClearMetadataForSyncedEntities() {
  for (const std::string& storage_key :
       change_processor()->GetAllTrackedStorageKeys()) {
    if (change_processor()->IsEntityUnsynced(storage_key)) {
      // "Unsynced" entities (i.e. those with local changes that still need to
      // be committed) have to be tracked, so *don't* clear their metadata.
      continue;
    }
    sync_metadata_database_->ClearSyncMetadata(syncer::HISTORY, storage_key);
    change_processor()->UntrackEntityForStorageKey(storage_key);
  }
}

std::string HistorySyncBridge::GetLocalCacheGuid() const {
  // Before the processor is tracking metadata, the cache GUID isn't known.
  DCHECK(change_processor()->IsTrackingMetadata());
  return change_processor()->TrackedCacheGuid();
}

std::vector<URLRow> HistorySyncBridge::QueryURLsForVisits(
    const std::vector<VisitRow>& visits) {
  std::vector<URLRow> urls;
  urls.reserve(visits.size());
  for (const VisitRow& visit : visits) {
    URLRow url;
    history_backend_->GetURLByID(visit.url_id, &url);
    urls.push_back(std::move(url));
  }
  return urls;
}

}  // namespace history
