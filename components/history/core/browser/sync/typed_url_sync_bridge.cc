// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/typed_url_sync_bridge.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/big_endian.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"
#include "net/base/url_util.h"

using sync_pb::TypedUrlSpecifics;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataChangeList;
using syncer::ModelError;
using syncer::ModelTypeChangeProcessor;
using syncer::MutableDataBatch;

namespace history {

namespace {

// The server backend can't handle arbitrarily large node sizes, so to keep
// the size under control we limit the visit array.
static const int kMaxTypedUrlVisits = 100;

// There's no limit on how many visits the history DB could have for a given
// typed URL, so we limit how many we fetch from the DB to avoid crashes due to
// running out of memory (http://crbug.com/89793). This value is different
// from kMaxTypedUrlVisits, as some of the visits fetched from the DB may be
// RELOAD visits, which will be stripped.
static const int kMaxVisitsToFetch = 1000;

// This is the threshold at which we start throttling sync updates for typed
// URLs - any URLs with a typed_count >= this threshold will be throttled.
static const int kTypedUrlVisitThrottleThreshold = 10;

// This is the multiple we use when throttling sync updates. If the multiple is
// N, we sync up every Nth update (i.e. when typed_count % N == 0).
static const int kTypedUrlVisitThrottleMultiple = 10;

// Enforce oldest to newest visit order.
static bool CheckVisitOrdering(const VisitVector& visits) {
  int64_t previous_visit_time = 0;
  for (auto visit = visits.begin(); visit != visits.end(); ++visit) {
    if (visit != visits.begin() &&
        previous_visit_time > visit->visit_time.ToInternalValue())
      return false;

    previous_visit_time = visit->visit_time.ToInternalValue();
  }
  return true;
}

std::string GetStorageKeyFromURLRow(const URLRow& row) {
  DCHECK_NE(row.id(), 0);
  std::string storage_key(sizeof(row.id()), 0);
  base::WriteBigEndian<URLID>(&storage_key[0], row.id());
  return storage_key;
}

bool HasTypedUrl(const VisitVector& visits) {
  auto typed_url_visit =
      std::find_if(visits.begin(), visits.end(), [](const VisitRow& visit) {
        return ui::PageTransitionCoreTypeIs(visit.transition,
                                            ui::PAGE_TRANSITION_TYPED);
      });
  return typed_url_visit != visits.end();
}

}  // namespace

TypedURLSyncBridge::TypedURLSyncBridge(
    HistoryBackend* history_backend,
    TypedURLSyncMetadataDatabase* sync_metadata_database,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)),
      history_backend_(history_backend),
      processing_syncer_changes_(false),
      sync_metadata_database_(sync_metadata_database),
      num_db_accesses_(0),
      num_db_errors_(0) {
  DCHECK(history_backend_);
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

TypedURLSyncBridge::~TypedURLSyncBridge() {}

std::unique_ptr<MetadataChangeList>
TypedURLSyncBridge::CreateMetadataChangeList() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      sync_metadata_database_, syncer::TYPED_URLS);
}

base::Optional<ModelError> TypedURLSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // Create a mapping of all local data by URLID. These will be narrowed down
  // by MergeURLWithSync() to include only the entries different from sync
  // server data.
  TypedURLMap new_db_urls;

  // Get all the visits and map the URLRows by URL.
  URLVisitVectorMap local_visit_vectors;

  if (!GetValidURLsAndVisits(&local_visit_vectors, &new_db_urls)) {
    return ModelError(
        FROM_HERE, "Could not get the typed_url entries from HistoryBackend.");
  }

  // New sync data organized for different write operations to history backend.
  URLRows new_synced_urls;
  URLRows updated_synced_urls;
  TypedURLVisitVector new_synced_visits;

  // Iterate through entity_data and check for all the urls that
  // sync already knows about. MergeURLWithSync() will remove urls that
  // are the same as the synced ones from |new_db_urls|.
  for (const std::unique_ptr<EntityChange>& entity_change : entity_data) {
    DCHECK(entity_change->data().specifics.has_typed_url());
    const TypedUrlSpecifics& specifics =
        entity_change->data().specifics.typed_url();
    if (ShouldIgnoreUrl(GURL(specifics.url())))
      continue;

    // Ignore old sync urls that don't have any transition data stored with
    // them, or transition data that does not match the visit data (will be
    // deleted later by GarbageCollectionDirective).
    if (specifics.visit_transitions_size() == 0 ||
        specifics.visit_transitions_size() != specifics.visits_size()) {
      DCHECK_EQ(specifics.visits_size(), specifics.visit_transitions_size());
      DLOG(WARNING)
          << "Ignoring obsolete sync url with no visit transition info.";

      continue;
    }
    MergeURLWithSync(specifics, &new_db_urls, &local_visit_vectors,
                     &new_synced_urls, &new_synced_visits,
                     &updated_synced_urls);
  }

  base::Optional<ModelError> error =
      WriteToHistoryBackend(&new_synced_urls, &updated_synced_urls, nullptr,
                            &new_synced_visits, nullptr);
  if (error)
    return error;

  // Update storage key here first, and then send updated typed URL to sync
  // below, otherwise processor will have duplicate entries.
  for (const std::unique_ptr<EntityChange>& entity_change : entity_data) {
    DCHECK(entity_change->data().specifics.has_typed_url());
    std::string storage_key = GetStorageKeyInternal(
        entity_change->data().specifics.typed_url().url());
    if (storage_key.empty()) {
      // ignore entity change
      change_processor()->UntrackEntityForClientTagHash(
          entity_change->data().client_tag_hash);
    } else {
      change_processor()->UpdateStorageKey(entity_change->data(), storage_key,
                                           metadata_change_list.get());
    }
  }

  // Send new/updated typed URL to sync.
  for (const auto& kv : new_db_urls) {
    SendTypedURLToProcessor(kv.second, local_visit_vectors[kv.first],
                            metadata_change_list.get());
  }

  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlMergeAndStartSyncingErrors",
                           GetErrorPercentage());
  ClearErrorStats();

  return static_cast<syncer::SyncMetadataStoreChangeList*>(
             metadata_change_list.get())
      ->TakeError();
}

base::Optional<ModelError> TypedURLSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  std::vector<GURL> pending_deleted_urls;
  TypedURLVisitVector new_synced_visits;
  VisitVector deleted_visits;
  URLRows updated_synced_urls;
  URLRows new_synced_urls;

  for (const std::unique_ptr<EntityChange>& entity_change : entity_changes) {
    if (entity_change->type() == EntityChange::ACTION_DELETE) {
      URLRow url_row;
      int64_t url_id = TypedURLSyncMetadataDatabase::StorageKeyToURLID(
          entity_change->storage_key());
      if (!history_backend_->GetURLByID(url_id, &url_row)) {
        // Ignoring the case that there is no matching URLRow with URLID
        // |url_id|.
        continue;
      }

      pending_deleted_urls.push_back(url_row.url());
      continue;
    }

    DCHECK(entity_change->data().specifics.has_typed_url());
    const TypedUrlSpecifics& specifics =
        entity_change->data().specifics.typed_url();

    GURL url(specifics.url());

    if (ShouldIgnoreUrl(url))
      continue;

    DCHECK(specifics.visits_size());
    sync_pb::TypedUrlSpecifics filtered_url = FilterExpiredVisits(specifics);
    if (filtered_url.visits_size() == 0)
      continue;

    UpdateFromSync(filtered_url, &new_synced_visits, &deleted_visits,
                   &updated_synced_urls, &new_synced_urls);
  }

  base::Optional<ModelError> error = WriteToHistoryBackend(
      &new_synced_urls, &updated_synced_urls, &pending_deleted_urls,
      &new_synced_visits, &deleted_visits);
  if (error)
    return error;

  // New entities were either ignored or written to history DB and assigned a
  // storage key. Notify processor about updated storage keys.
  for (const std::unique_ptr<EntityChange>& entity_change : entity_changes) {
    if (entity_change->type() == EntityChange::ACTION_ADD) {
      std::string storage_key = GetStorageKeyInternal(
          entity_change->data().specifics.typed_url().url());
      if (storage_key.empty()) {
        // ignore entity change
        change_processor()->UntrackEntityForClientTagHash(
            entity_change->data().client_tag_hash);
      } else {
        change_processor()->UpdateStorageKey(entity_change->data(), storage_key,
                                             metadata_change_list.get());
      }
    }
  }

  return static_cast<syncer::SyncMetadataStoreChangeList*>(
             metadata_change_list.get())
      ->TakeError();
}

void TypedURLSyncBridge::GetData(StorageKeyList storage_keys,
                                 DataCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  auto batch = std::make_unique<MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    URLRow url_row;
    URLID url_id = TypedURLSyncMetadataDatabase::StorageKeyToURLID(key);

    ++num_db_accesses_;
    if (!history_backend_->GetURLByID(url_id, &url_row)) {
      // Ignoring the case which no matching URLRow with URLID |url_id|.
      DLOG(ERROR) << "Could not find URL for id: " << url_id;
      continue;
    }

    VisitVector visits_vector;
    if (!FixupURLAndGetVisits(&url_row, &visits_vector))
      continue;
    std::unique_ptr<syncer::EntityData> entity_data =
        CreateEntityData(url_row, visits_vector);
    if (!entity_data) {
      // Cannot create EntityData, ex. no TYPED visits.
      continue;
    }

    batch->Put(key, std::move(entity_data));
  }

  std::move(callback).Run(std::move(batch));
}

void TypedURLSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  URLRows typed_urls;
  ++num_db_accesses_;
  if (!history_backend_->GetAllTypedURLs(&typed_urls)) {
    ++num_db_errors_;
    change_processor()->ReportError(
        {FROM_HERE, "Could not get the typed_url entries."});
    return;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (URLRow& url : typed_urls) {
    VisitVector visits_vector;
    if (!FixupURLAndGetVisits(&url, &visits_vector))
      continue;
    std::unique_ptr<syncer::EntityData> entity_data =
        CreateEntityData(url, visits_vector);
    if (!entity_data) {
      // Cannot create EntityData, ex. no TYPED visits.
      continue;
    }

    batch->Put(GetStorageKeyFromURLRow(url), std::move(entity_data));
  }

  std::move(callback).Run(std::move(batch));
}

// Must be exactly the value of GURL::spec() for backwards comparability with
// the previous (Directory + SyncableService) iteration of sync integration.
// This can be large but it is assumed that this is not held in memory at steady
// state.
std::string TypedURLSyncBridge::GetClientTag(const EntityData& entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(entity_data.specifics.has_typed_url())
      << "EntityData does not have typed urls specifics.";

  return entity_data.specifics.typed_url().url();
}

// Prefer to use URLRow::id() to uniquely identify entities when coordinating
// with sync because it has a significantly low memory cost than a URL.
std::string TypedURLSyncBridge::GetStorageKey(const EntityData& entity_data) {
  NOTREACHED() << "TypedURLSyncBridge do not support GetStorageKey.";
  return std::string();
}

bool TypedURLSyncBridge::SupportsGetStorageKey() const {
  return false;
}

void TypedURLSyncBridge::OnURLVisited(HistoryBackend* history_backend,
                                      ui::PageTransition transition,
                                      const URLRow& row,
                                      const RedirectList& redirects,
                                      base::Time visit_time) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(sync_metadata_database_);
  DCHECK_GE(row.typed_count(), 0);

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.

  if (!change_processor()->IsTrackingMetadata())
    return;  // Sync processor not yet ready, don't sync.
  if (!ShouldSyncVisit(row.typed_count(), transition))
    return;

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  UpdateSyncFromLocal(row, /*is_from_expiration=*/false,
                      metadata_change_list.get());
}

void TypedURLSyncBridge::OnURLsModified(HistoryBackend* history_backend,
                                        const URLRows& changed_urls,
                                        bool is_from_expiration) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(sync_metadata_database_);

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.

  if (!change_processor()->IsTrackingMetadata())
    return;  // Sync processor not yet ready, don't sync.

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  for (const auto& row : changed_urls) {
    DCHECK_GE(row.typed_count(), 0);
    // If there were any errors updating the sync node, just ignore them and
    // continue on to process the next URL.
    UpdateSyncFromLocal(row, is_from_expiration, metadata_change_list.get());
  }
}

void TypedURLSyncBridge::OnURLsDeleted(HistoryBackend* history_backend,
                                       bool all_history,
                                       bool expired,
                                       const URLRows& deleted_rows,
                                       const std::set<GURL>& favicon_urls) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(sync_metadata_database_);

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.

  if (!change_processor()->IsTrackingMetadata())
    return;  // Sync processor not yet ready, don't sync.

  // Ignore URLs expired due to old age (we don't want to sync them as deletions
  // to avoid extra traffic up to the server, and also to make sure that a
  // client with a bad clock setting won't go on an expiration rampage and
  // delete all history from every client). The server will gracefully age out
  // the sync DB entries when they've been idle for long enough.
  if (expired) {
    // Delete metadata from the DB and ask the processor to untrack the entries.
    for (const URLRow& row : deleted_rows) {
      ExpireMetadataForURL(row);
    }
    return;
  }

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();

  if (all_history) {
    auto batch = std::make_unique<syncer::MetadataBatch>();
    if (!sync_metadata_database_->GetAllSyncMetadata(batch.get())) {
      change_processor()->ReportError({FROM_HERE,
                                       "Failed reading typed url metadata from "
                                       "TypedURLSyncMetadataDatabase."});
      return;
    }

    syncer::EntityMetadataMap metadata_map(batch->TakeAllMetadata());
    for (const auto& kv : metadata_map) {
      change_processor()->Delete(kv.first, metadata_change_list.get());
    }
  } else {
    // Delete rows.
    for (const URLRow& row : deleted_rows) {
      std::string storage_key = GetStorageKeyFromURLRow(row);
      change_processor()->Delete(storage_key, metadata_change_list.get());
    }
  }
}

void TypedURLSyncBridge::Init() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  history_backend_observer_.Add(history_backend_);
  LoadMetadata();
}

void TypedURLSyncBridge::OnDatabaseError() {
  sync_metadata_database_ = nullptr;
  change_processor()->ReportError(
      {FROM_HERE, "HistoryDatabase encountered error"});
}

int TypedURLSyncBridge::GetErrorPercentage() const {
  return num_db_accesses_ ? (100 * num_db_errors_ / num_db_accesses_) : 0;
}

// static
bool TypedURLSyncBridge::WriteToTypedUrlSpecifics(
    const URLRow& url,
    const VisitVector& visits,
    TypedUrlSpecifics* typed_url) {
  DCHECK(!url.last_visit().is_null());
  DCHECK(!visits.empty());
  DCHECK_EQ(url.last_visit().ToInternalValue(),
            visits.back().visit_time.ToInternalValue());

  typed_url->set_url(url.url().spec());
  typed_url->set_title(base::UTF16ToUTF8(url.title()));
  typed_url->set_hidden(url.hidden());

  DCHECK(CheckVisitOrdering(visits));

  bool only_typed = false;
  int skip_count = 0;

  if (!HasTypedUrl(visits)) {
    // This URL has no TYPED visits, don't sync it
    return false;
  }

  if (visits.size() > static_cast<size_t>(kMaxTypedUrlVisits)) {
    int typed_count = 0;
    int total = 0;
    // Walk the passed-in visit vector and count the # of typed visits.
    for (VisitRow visit : visits) {
      // We ignore reload visits.
      if (PageTransitionCoreTypeIs(visit.transition,
                                   ui::PAGE_TRANSITION_RELOAD)) {
        continue;
      }
      ++total;
      if (PageTransitionCoreTypeIs(visit.transition,
                                   ui::PAGE_TRANSITION_TYPED)) {
        ++typed_count;
      }
    }

    // We should have at least one typed visit. This can sometimes happen if
    // the history DB has an inaccurate count for some reason (there's been
    // bugs in the history code in the past which has left users in the wild
    // with incorrect counts - http://crbug.com/84258).
    DCHECK_GT(typed_count, 0);

    if (typed_count > kMaxTypedUrlVisits) {
      only_typed = true;
      skip_count = typed_count - kMaxTypedUrlVisits;
    } else if (total > kMaxTypedUrlVisits) {
      skip_count = total - kMaxTypedUrlVisits;
    }
  }

  for (const auto& visit : visits) {
    // Skip reload visits.
    if (PageTransitionCoreTypeIs(visit.transition, ui::PAGE_TRANSITION_RELOAD))
      continue;

    // If we only have room for typed visits, then only add typed visits.
    if (only_typed && !PageTransitionCoreTypeIs(visit.transition,
                                                ui::PAGE_TRANSITION_TYPED)) {
      continue;
    }

    if (skip_count > 0) {
      // We have too many entries to fit, so we need to skip the oldest ones.
      // Only skip typed URLs if there are too many typed URLs to fit.
      if (only_typed || !PageTransitionCoreTypeIs(visit.transition,
                                                  ui::PAGE_TRANSITION_TYPED)) {
        --skip_count;
        continue;
      }
    }
    typed_url->add_visits(visit.visit_time.ToInternalValue());
    typed_url->add_visit_transitions(visit.transition);
  }
  DCHECK_EQ(skip_count, 0);

  CHECK_GT(typed_url->visits_size(), 0);
  CHECK_LE(typed_url->visits_size(), kMaxTypedUrlVisits);
  CHECK_EQ(typed_url->visits_size(), typed_url->visit_transitions_size());

  return true;
}

// static
TypedURLSyncBridge::MergeResult TypedURLSyncBridge::MergeUrls(
    const TypedUrlSpecifics& sync_url,
    const URLRow& url,
    VisitVector* visits,
    URLRow* new_url,
    std::vector<VisitInfo>* new_visits) {
  DCHECK(new_url);
  DCHECK_EQ(sync_url.url(), url.url().spec());
  DCHECK_EQ(sync_url.url(), new_url->url().spec());
  DCHECK(visits->size());
  DCHECK_GT(sync_url.visits_size(), 0);
  CHECK_EQ(sync_url.visits_size(), sync_url.visit_transitions_size());

  // Convert these values only once.
  base::string16 sync_url_title(base::UTF8ToUTF16(sync_url.title()));
  base::Time sync_url_last_visit = base::Time::FromInternalValue(
      sync_url.visits(sync_url.visits_size() - 1));

  // This is a bitfield representing what we'll need to update with the output
  // value.
  MergeResult different = DIFF_NONE;

  // Check if the non-incremented values changed.
  if ((sync_url_title != url.title()) || (sync_url.hidden() != url.hidden())) {
    // Use the values from the most recent visit.
    if (sync_url_last_visit >= url.last_visit()) {
      new_url->set_title(sync_url_title);
      new_url->set_hidden(sync_url.hidden());
      different |= DIFF_LOCAL_ROW_CHANGED;
    } else {
      new_url->set_title(url.title());
      new_url->set_hidden(url.hidden());
      different |= DIFF_UPDATE_NODE;
    }
  } else {
    // No difference.
    new_url->set_title(url.title());
    new_url->set_hidden(url.hidden());
  }

  size_t sync_url_num_visits = sync_url.visits_size();
  size_t history_num_visits = visits->size();
  size_t sync_url_visit_index = 0;
  size_t history_visit_index = 0;
  base::Time earliest_history_time = (*visits)[0].visit_time;
  // Walk through the two sets of visits and figure out if any new visits were
  // added on either side.
  while (sync_url_visit_index < sync_url_num_visits ||
         history_visit_index < history_num_visits) {
    // Time objects are initialized to "earliest possible time".
    base::Time sync_url_time, history_time;
    if (sync_url_visit_index < sync_url_num_visits)
      sync_url_time =
          base::Time::FromInternalValue(sync_url.visits(sync_url_visit_index));
    if (history_visit_index < history_num_visits)
      history_time = (*visits)[history_visit_index].visit_time;
    if (sync_url_visit_index >= sync_url_num_visits ||
        (history_visit_index < history_num_visits &&
         sync_url_time > history_time)) {
      // We found a visit in the history DB that doesn't exist in the sync DB,
      // so mark the sync_url as modified so the caller will update the sync
      // node.
      different |= DIFF_UPDATE_NODE;
      ++history_visit_index;
    } else if (history_visit_index >= history_num_visits ||
               sync_url_time < history_time) {
      // Found a visit in the sync node that doesn't exist in the history DB, so
      // add it to our list of new visits and set the appropriate flag so the
      // caller will update the history DB.
      // If the sync_url visit is older than any existing visit in the history
      // DB, don't re-add it - this keeps us from resurrecting visits that were
      // aged out locally.
      //
      // TODO(sync): This extra check should be unnecessary now that filtering
      // expired visits is performed separately. Non-expired visits older than
      // the earliest existing history visits should still be synced, so this
      // check should be removed.
      if (sync_url_time > earliest_history_time) {
        different |= DIFF_LOCAL_VISITS_ADDED;
        new_visits->push_back(VisitInfo(
            sync_url_time, ui::PageTransitionFromInt(sync_url.visit_transitions(
                               sync_url_visit_index))));
      }
      // This visit is added to visits below.
      ++sync_url_visit_index;
    } else {
      // Same (already synced) entry found in both DBs - no need to do anything.
      ++sync_url_visit_index;
      ++history_visit_index;
    }
  }

  DCHECK(CheckVisitOrdering(*visits));
  if (different & DIFF_LOCAL_VISITS_ADDED) {
    // If the server does not have the same visits as the local db, then the
    // new visits from the server need to be added to the vector containing
    // local visits. These visits will be passed to the server.
    // Insert new visits into the appropriate place in the visits vector.
    auto visit_ix = visits->begin();
    for (auto new_visit = new_visits->begin(); new_visit != new_visits->end();
         ++new_visit) {
      while (visit_ix != visits->end() &&
             new_visit->first > visit_ix->visit_time) {
        ++visit_ix;
      }
      visit_ix = visits->insert(
          visit_ix,
          VisitRow(url.id(), new_visit->first, 0, new_visit->second, 0,
                   HistoryBackend::IsTypedIncrement(new_visit->second)));
      ++visit_ix;
    }
  }
  DCHECK(CheckVisitOrdering(*visits));

  new_url->set_last_visit(visits->back().visit_time);
  return different;
}

// static
void TypedURLSyncBridge::DiffVisits(
    const VisitVector& history_visits,
    const sync_pb::TypedUrlSpecifics& sync_specifics,
    std::vector<VisitInfo>* new_visits,
    VisitVector* removed_visits) {
  DCHECK(new_visits);
  size_t old_visit_count = history_visits.size();
  size_t new_visit_count = sync_specifics.visits_size();
  size_t old_index = 0;
  size_t new_index = 0;
  while (old_index < old_visit_count && new_index < new_visit_count) {
    base::Time new_visit_time =
        base::Time::FromInternalValue(sync_specifics.visits(new_index));
    if (history_visits[old_index].visit_time < new_visit_time) {
      if (new_index > 0 && removed_visits) {
        // If there are visits missing from the start of the node, that
        // means that they were probably clipped off due to our code that
        // limits the size of the sync nodes - don't delete them from our
        // local history.
        removed_visits->push_back(history_visits[old_index]);
      }
      ++old_index;
    } else if (history_visits[old_index].visit_time > new_visit_time) {
      new_visits->push_back(VisitInfo(
          new_visit_time, ui::PageTransitionFromInt(
                              sync_specifics.visit_transitions(new_index))));
      ++new_index;
    } else {
      ++old_index;
      ++new_index;
    }
  }

  if (removed_visits) {
    for (; old_index < old_visit_count; ++old_index) {
      removed_visits->push_back(history_visits[old_index]);
    }
  }

  for (; new_index < new_visit_count; ++new_index) {
    new_visits->push_back(VisitInfo(
        base::Time::FromInternalValue(sync_specifics.visits(new_index)),
        ui::PageTransitionFromInt(
            sync_specifics.visit_transitions(new_index))));
  }
}

// static
void TypedURLSyncBridge::UpdateURLRowFromTypedUrlSpecifics(
    const TypedUrlSpecifics& typed_url,
    URLRow* new_url) {
  DCHECK_GT(typed_url.visits_size(), 0);
  CHECK_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  if (!new_url->url().is_valid()) {
    new_url->set_url(GURL(typed_url.url()));
  }
  new_url->set_title(base::UTF8ToUTF16(typed_url.title()));
  new_url->set_hidden(typed_url.hidden());
  // Only provide the initial value for the last_visit field - after that, let
  // the history code update the last_visit field on its own.
  if (new_url->last_visit().is_null()) {
    new_url->set_last_visit(base::Time::FromInternalValue(
        typed_url.visits(typed_url.visits_size() - 1)));
  }
}

void TypedURLSyncBridge::LoadMetadata() {
  if (!history_backend_ || !sync_metadata_database_) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load TypedURLSyncMetadataDatabase."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!sync_metadata_database_->GetAllSyncMetadata(batch.get())) {
    change_processor()->ReportError({FROM_HERE,
                                     "Failed reading typed url metadata from "
                                     "TypedURLSyncMetadataDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

void TypedURLSyncBridge::ClearErrorStats() {
  num_db_accesses_ = 0;
  num_db_errors_ = 0;
}

void TypedURLSyncBridge::MergeURLWithSync(
    const sync_pb::TypedUrlSpecifics& server_typed_url,
    TypedURLMap* local_typed_urls,
    URLVisitVectorMap* local_visit_vectors,
    URLRows* new_synced_urls,
    TypedURLVisitVector* new_synced_visits,
    URLRows* updated_synced_urls) {
  DCHECK_NE(0, server_typed_url.visits_size());
  DCHECK_EQ(server_typed_url.visits_size(),
            server_typed_url.visit_transitions_size());

  // Ignore empty urls.
  if (server_typed_url.url().empty()) {
    DVLOG(1) << "Ignoring empty URL in sync DB";
    return;
  }
  // Now, get rid of the expired visits. If there are no un-expired visits
  // left, ignore this url - any local data should just replace it.
  TypedUrlSpecifics sync_url = FilterExpiredVisits(server_typed_url);
  if (sync_url.visits_size() == 0) {
    DVLOG(1) << "Ignoring expired URL in sync DB: " << sync_url.url();
    return;
  }

  // Check if local db already has the url from sync.
  auto it = local_typed_urls->find(GURL(sync_url.url()));
  if (it == local_typed_urls->end()) {
    // There are no matching typed urls from the local db, check for untyped
    URLRow untyped_url(GURL(sync_url.url()));

    // The URL may still exist in the local db if it is an untyped url.
    // An untyped url will transition to a typed url after receiving visits
    // from sync, and sync should receive any visits already existing locally
    // for the url, so the full list of visits is consistent.
    bool is_existing_url =
        history_backend_->GetURL(untyped_url.url(), &untyped_url);
    if (is_existing_url) {
      // Add a new entry to |local_typed_urls|, and set the iterator to it.
      VisitVector untyped_visits;
      if (!FixupURLAndGetVisits(&untyped_url, &untyped_visits)) {
        return;
      }
      (*local_visit_vectors)[untyped_url.url()] = untyped_visits;

      // Store row info that will be used to update sync's visits.
      (*local_typed_urls)[untyped_url.url()] = untyped_url;

      // Set iterator |it| to point to this entry.
      it = local_typed_urls->find(untyped_url.url());
      DCHECK(it != local_typed_urls->end());
      // Continue with merge below.
    } else {
      // The url is new to the local history DB.
      // Create new db entry for url.
      URLRow new_url(GURL(sync_url.url()));
      UpdateURLRowFromTypedUrlSpecifics(sync_url, &new_url);
      new_synced_urls->push_back(new_url);

      // Add entries for url visits.
      std::vector<VisitInfo> added_visits;
      size_t visit_count = sync_url.visits_size();

      for (size_t index = 0; index < visit_count; ++index) {
        base::Time visit_time =
            base::Time::FromInternalValue(sync_url.visits(index));
        ui::PageTransition transition =
            ui::PageTransitionFromInt(sync_url.visit_transitions(index));
        added_visits.push_back(VisitInfo(visit_time, transition));
      }
      new_synced_visits->push_back(
          std::pair<GURL, std::vector<VisitInfo>>(new_url.url(), added_visits));
      return;
    }
  }

  // Same URL exists in sync data and in history data - compare the
  // entries to see if there's any difference.
  VisitVector& visits = (*local_visit_vectors)[it->first];
  std::vector<VisitInfo> added_visits;

  // Empty URLs should be filtered out by ShouldIgnoreUrl() previously.
  DCHECK(!it->second.url().spec().empty());

  // Initialize fields in |new_url| to the same values as the fields in
  // the existing URLRow in the history DB. This is needed because we
  // overwrite the existing value in WriteToHistoryBackend(), but some of
  // the values in that structure are not synced (like typed_count).
  URLRow new_url(it->second);

  MergeResult difference =
      MergeUrls(sync_url, it->second, &visits, &new_url, &added_visits);

  if (difference != DIFF_NONE) {
    it->second = new_url;
    if (difference & DIFF_UPDATE_NODE) {
      // We don't want to resurrect old visits that have been aged out by
      // other clients, so remove all visits that are older than the
      // earliest existing visit in the sync node.
      //
      // TODO(sync): This logic should be unnecessary now that filtering of
      // expired visits is performed separately. Non-expired visits older than
      // the earliest existing sync visits should still be synced, so this
      // logic should be removed.
      if (sync_url.visits_size() > 0) {
        base::Time earliest_visit =
            base::Time::FromInternalValue(sync_url.visits(0));
        for (auto i = visits.begin();
             i != visits.end() && i->visit_time < earliest_visit;) {
          i = visits.erase(i);
        }
        // Should never be possible to delete all the items, since the
        // visit vector contains newer local visits it will keep and/or the
        // visits in typed_url.visits newer than older local visits.
        DCHECK_GT(visits.size(), 0U);
      }
      DCHECK_EQ(new_url.last_visit().ToInternalValue(),
                visits.back().visit_time.ToInternalValue());
    }
    if (difference & DIFF_LOCAL_ROW_CHANGED) {
      // Add entry to updated_synced_urls to update the local db.
      DCHECK_EQ(it->second.id(), new_url.id());
      updated_synced_urls->push_back(new_url);
    }
    if (difference & DIFF_LOCAL_VISITS_ADDED) {
      // Add entry with new visits to new_synced_visits to update the local db.
      new_synced_visits->push_back(
          std::pair<GURL, std::vector<VisitInfo>>(it->first, added_visits));
    }
  } else {
    // No difference in urls, erase from map
    local_typed_urls->erase(it);
  }
}

void TypedURLSyncBridge::UpdateFromSync(
    const sync_pb::TypedUrlSpecifics& typed_url,
    TypedURLVisitVector* visits_to_add,
    VisitVector* visits_to_remove,
    URLRows* updated_urls,
    URLRows* new_urls) {
  URLRow new_url(GURL(typed_url.url()));
  VisitVector existing_visits;
  bool existing_url = history_backend_->GetURL(new_url.url(), &new_url);
  if (existing_url) {
    // This URL already exists locally - fetch the visits so we can
    // merge them below.
    if (!FixupURLAndGetVisits(&new_url, &existing_visits)) {
      return;
    }
  }
  visits_to_add->push_back(std::pair<GURL, std::vector<VisitInfo>>(
      new_url.url(), std::vector<VisitInfo>()));

  // Update the URL with information from the typed URL.
  UpdateURLRowFromTypedUrlSpecifics(typed_url, &new_url);

  // Figure out which visits we need to add.
  DiffVisits(existing_visits, typed_url, &visits_to_add->back().second,
             visits_to_remove);

  if (existing_url) {
    updated_urls->push_back(new_url);
  } else {
    new_urls->push_back(new_url);
  }
}

void TypedURLSyncBridge::UpdateSyncFromLocal(
    URLRow row,
    bool is_from_expiration,
    MetadataChangeList* metadata_change_list) {
  if (ShouldIgnoreUrl(row.url()))
    return;

  // Get the visits for this node.
  VisitVector visit_vector;
  if (!FixupURLAndGetVisits(&row, &visit_vector)) {
    return;
  }

  std::string storage_key = GetStorageKeyFromURLRow(row);

  if (HasTypedUrl(visit_vector)) {
    SendTypedURLToProcessor(row, visit_vector, metadata_change_list);
  } else {
    // If the URL has no typed visits any more we should get rid of it. It is
    // possible that this URL never had typed visits and thus it has no sync
    // entity and no sync metadata. We do not need to check for this case
    // as all the code below is no-op if there is no sync metadata for |row|.
    if (is_from_expiration) {
      // Only remove its metadata as we do not sync up deletions for expired
      // entities (see the comment in OnURLsDeleted()).
      ExpireMetadataForURL(row);
    } else {
      // This change is caused by the user explicitly removing some visits, we
      // should also remove the entity from sync.
      change_processor()->Delete(storage_key, metadata_change_list);
    }
  }
}

void TypedURLSyncBridge::ExpireMetadataForURL(const URLRow& row) {
  std::string storage_key = GetStorageKeyFromURLRow(row);
  // The following functions need to tolerate if there exists no metadata
  // for |storage_key| as we might call this function multiple times for a given
  // url.
  sync_metadata_database_->ClearSyncMetadata(syncer::TYPED_URLS, storage_key);
  change_processor()->UntrackEntityForStorageKey(storage_key);
}

base::Optional<ModelError> TypedURLSyncBridge::WriteToHistoryBackend(
    const URLRows* new_urls,
    const URLRows* updated_urls,
    const std::vector<GURL>* deleted_urls,
    const TypedURLVisitVector* new_visits,
    const VisitVector* deleted_visits) {
  DCHECK_EQ(processing_syncer_changes_, false);
  // Set flag to stop accepting history change notifications from backend.
  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  if (deleted_urls && !deleted_urls->empty())
    history_backend_->DeleteURLs(*deleted_urls);

  if (new_urls) {
    history_backend_->AddPagesWithDetails(*new_urls, SOURCE_SYNCED);
  }

  if (updated_urls) {
    ++num_db_accesses_;
    // This is an existing entry in the URL database. We don't verify the
    // visit_count or typed_count values here, because either one (or both)
    // could be zero in the case of bookmarks, or in the case of a URL
    // transitioning from non-typed to typed as a result of this sync.
    // In the field we sometimes run into errors on specific URLs. It's OK
    // to just continue on (we can try writing again on the next model
    // association).
    size_t num_successful_updates = history_backend_->UpdateURLs(*updated_urls);
    num_db_errors_ += updated_urls->size() - num_successful_updates;
  }

  if (new_visits) {
    for (const auto& visits : *new_visits) {
      // If there are no visits to add, just skip this.
      if (visits.second.empty())
        continue;
      ++num_db_accesses_;
      if (!history_backend_->AddVisits(visits.first, visits.second,
                                       SOURCE_SYNCED)) {
        ++num_db_errors_;
        return ModelError(FROM_HERE, "Could not add visits to HistoryBackend.");
      }
    }
  }

  if (deleted_visits) {
    ++num_db_accesses_;
    if (!history_backend_->RemoveVisits(*deleted_visits)) {
      ++num_db_errors_;
      return ModelError(FROM_HERE,
                        "Could not remove visits from HistoryBackend.");
      // This is bad news, since it means we may end up resurrecting history
      // entries on the next reload. It's unavoidable so we'll just keep on
      // syncing.
    }
  }

  return {};
}

TypedUrlSpecifics TypedURLSyncBridge::FilterExpiredVisits(
    const TypedUrlSpecifics& source) {
  // Make a copy of the source, then regenerate the visits.
  TypedUrlSpecifics specifics(source);
  specifics.clear_visits();
  specifics.clear_visit_transitions();
  for (int i = 0; i < source.visits_size(); ++i) {
    base::Time time = base::Time::FromInternalValue(source.visits(i));
    if (!history_backend_->IsExpiredVisitTime(time)) {
      specifics.add_visits(source.visits(i));
      specifics.add_visit_transitions(source.visit_transitions(i));
    }
  }
  DCHECK(specifics.visits_size() == specifics.visit_transitions_size());
  return specifics;
}

bool TypedURLSyncBridge::ShouldIgnoreUrl(const GURL& url) {
  // Ignore empty URLs. Not sure how this can happen (maybe import from other
  // busted browsers, or misuse of the history API, or just plain bugs) but we
  // can't deal with them.
  if (url.spec().empty())
    return true;

  // Ignore local file URLs.
  if (url.SchemeIsFile())
    return true;

  // Ignore localhost URLs.
  if (net::IsLocalhost(url))
    return true;

  // Ignore username and password, since history backend will remove user name
  // and password in URLDatabase::GURLToDatabaseURL and send username/password
  // removed url to sync later.
  if (url.has_username() || url.has_password())
    return true;

  return false;
}

bool TypedURLSyncBridge::ShouldIgnoreVisits(const VisitVector& visits) {
  // We ignore URLs that were imported, but have never been visited by
  // chromium.
  static const int kFirstImportedSource = SOURCE_FIREFOX_IMPORTED;
  VisitSourceMap map;
  if (!history_backend_->GetVisitsSource(visits, &map))
    return false;  // If we can't read the visit, assume it's not imported.

  // Walk the list of visits and look for a non-imported item.
  for (const auto& visit : visits) {
    if (map.count(visit.visit_id) == 0 ||
        map[visit.visit_id] < kFirstImportedSource) {
      return false;
    }
  }
  // We only saw imported visits, so tell the caller to ignore them.
  return true;
}

bool TypedURLSyncBridge::ShouldSyncVisit(int typed_count,
                                         ui::PageTransition transition) {
  // Just use an ad-hoc criteria to determine whether to ignore this
  // notification. For most users, the distribution of visits is roughly a bell
  // curve with a long tail - there are lots of URLs with < 5 visits so we want
  // to make sure we sync up every visit to ensure the proper ordering of
  // suggestions. But there are relatively few URLs with > 10 visits, and those
  // tend to be more broadly distributed such that there's no need to sync up
  // every visit to preserve their relative ordering.
  return (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
          (typed_count < kTypedUrlVisitThrottleThreshold ||
           (typed_count % kTypedUrlVisitThrottleMultiple) == 0));
}

bool TypedURLSyncBridge::FixupURLAndGetVisits(URLRow* url,
                                              VisitVector* visits) {
  ++num_db_accesses_;
  if (!history_backend_->GetMostRecentVisitsForURL(url->id(), kMaxVisitsToFetch,
                                                   visits)) {
    ++num_db_errors_;
    // Couldn't load the visits for this URL due to some kind of DB error.
    // Don't bother writing this URL to the history DB (if we ignore the
    // error and continue, we might end up duplicating existing visits).
    DLOG(ERROR) << "Could not load visits for url: " << url->url();
    return false;
  }

  // Sometimes (due to a bug elsewhere in the history or sync code, or due to
  // a crash between adding a URL to the history database and updating the
  // visit DB) the visit vector for a URL can be empty. If this happens, just
  // create a new visit whose timestamp is the same as the last_visit time.
  // This is a workaround for http://crbug.com/84258.
  if (visits->empty()) {
    DVLOG(1) << "Found empty visits for URL: " << url->url();
    if (url->last_visit().is_null()) {
      // If modified URL is bookmarked, history backend treats it as modified
      // even if all its visits are deleted. Return false to stop further
      // processing because sync expects valid visit time for modified entry.
      return false;
    }

    VisitRow visit(url->id(), url->last_visit(), 0, ui::PAGE_TRANSITION_TYPED,
                   0, true);
    visits->push_back(visit);
  }

  // GetMostRecentVisitsForURL() returns the data in the opposite order that
  // we need it, so reverse it.
  std::reverse(visits->begin(), visits->end());

  // Sometimes, the last_visit field in the URL doesn't match the timestamp of
  // the last visit in our visit array (they come from different tables, so
  // crashes/bugs can cause them to mismatch), so just set it here.
  url->set_last_visit(visits->back().visit_time);
  DCHECK(CheckVisitOrdering(*visits));

  // Removes all visits that are older than the current expiration time. Visits
  // are in ascending order now, so we can check from beginning to check how
  // many expired visits.
  size_t num_expired_visits = 0;
  for (auto& visit : *visits) {
    base::Time time = visit.visit_time;
    if (history_backend_->IsExpiredVisitTime(time)) {
      ++num_expired_visits;
    } else {
      break;
    }
  }
  if (num_expired_visits != 0) {
    if (num_expired_visits == visits->size()) {
      DVLOG(1) << "All visits are expired for url: " << url->url();
      visits->clear();
      return false;
    }
    visits->erase(visits->begin(), visits->begin() + num_expired_visits);
  }
  DCHECK(CheckVisitOrdering(*visits));

  return true;
}

std::unique_ptr<EntityData> TypedURLSyncBridge::CreateEntityData(
    const URLRow& row,
    const VisitVector& visits) {
  auto entity_data = std::make_unique<EntityData>();
  TypedUrlSpecifics* specifics = entity_data->specifics.mutable_typed_url();

  if (!WriteToTypedUrlSpecifics(row, visits, specifics)) {
    // Cannot write to specifics, ex. no TYPED visits.
    return nullptr;
  }
  entity_data->name = row.url().spec();
  return entity_data;
}

bool TypedURLSyncBridge::GetValidURLsAndVisits(URLVisitVectorMap* url_to_visit,
                                               TypedURLMap* url_to_urlrow) {
  DCHECK(url_to_visit);
  DCHECK(url_to_urlrow);

  URLRows local_typed_urls;
  ++num_db_accesses_;
  if (!history_backend_->GetAllTypedURLs(&local_typed_urls)) {
    ++num_db_errors_;
    return false;
  }
  for (URLRow& url : local_typed_urls) {
    DCHECK_EQ(0U, url_to_visit->count(url.url()));
    if (!FixupURLAndGetVisits(&url, &((*url_to_visit)[url.url()])) ||
        ShouldIgnoreUrl(url.url()) ||
        ShouldIgnoreVisits((*url_to_visit)[url.url()])) {
      // Ignore this URL if we couldn't load the visits or if there's some
      // other problem with it (it was empty, or imported and never visited).
    } else {
      // Add url to url_to_urlrow.
      (*url_to_urlrow)[url.url()] = url;
    }
  }
  return true;
}

std::string TypedURLSyncBridge::GetStorageKeyInternal(const std::string& url) {
  DCHECK(history_backend_);

  URLRow existing_url;
  ++num_db_accesses_;
  bool is_existing_url = history_backend_->GetURL(GURL(url), &existing_url);

  if (!is_existing_url) {
    // The typed url did not save to local history database, so return empty
    // string.
    return std::string();
  }

  return GetStorageKeyFromURLRow(existing_url);
}

void TypedURLSyncBridge::SendTypedURLToProcessor(
    const URLRow& row,
    const VisitVector& visits,
    MetadataChangeList* metadata_change_list) {
  DCHECK(!visits.empty());
  DCHECK(metadata_change_list);

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateEntityData(row, visits);
  if (!entity_data) {
    // Cannot create EntityData, ex. no TYPED visits.
    return;
  }

  std::string storage_key = GetStorageKeyFromURLRow(row);
  change_processor()->Put(storage_key, std::move(entity_data),
                          metadata_change_list);
}

}  // namespace history
