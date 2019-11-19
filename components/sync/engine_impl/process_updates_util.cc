// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/process_updates_util.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine_impl/syncer_proto_util.h"
#include "components/sync/engine_impl/syncer_types.h"
#include "components/sync/engine_impl/syncer_util.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/syncable_model_neutral_write_transaction.h"
#include "components/sync/syncable/syncable_proto_util.h"
#include "components/sync/syncable/syncable_util.h"

namespace syncer {

using syncable::GET_BY_ID;

namespace {

// This function attempts to determine whether or not this update is genuinely
// new, or if it is a reflection of one of our own commits.
//
// There is a known inaccuracy in its implementation.  If this update ends up
// being applied to a local item with a different ID, we will count the change
// as being a non-reflection update.  Fortunately, the server usually updates
// our IDs correctly in its commit response, so a new ID during GetUpdate should
// be rare.
//
// The only scenarios I can think of where this might happen are:
// - We commit a  new item to the server, but we don't persist the
// server-returned new ID to the database before we shut down.  On the GetUpdate
// following the next restart, we will receive an update from the server that
// updates its local ID.
// - When two attempts to create an item with identical UNIQUE_CLIENT_TAG values
// collide at the server.  I have seen this in testing.  When it happens, the
// test server will send one of the clients a response to upate its local ID so
// that both clients will refer to the item using the same ID going forward.  In
// this case, we're right to assume that the update is not a reflection.
//
// For more information, see FindLocalIdToUpdate().
bool UpdateContainsNewVersion(syncable::BaseTransaction* trans,
                              const sync_pb::SyncEntity& update) {
  int64_t existing_version = -1;  // The server always sends positive versions.
  syncable::Entry existing_entry(trans, GET_BY_ID,
                                 SyncableIdFromProto(update.id_string()));
  if (existing_entry.good())
    existing_version = existing_entry.GetBaseVersion();

  if (!existing_entry.good() && update.deleted()) {
    // There are several possible explanations for this.  The most common cases
    // will be first time sync and the redelivery of deletions we've already
    // synced, accepted, and purged from our database.  In either case, the
    // update is useless to us.  Let's count them all as "not new", even though
    // that may not always be entirely accurate.
    return false;
  }

  if (existing_entry.good() && !existing_entry.GetUniqueClientTag().empty() &&
      existing_entry.GetIsDel() && update.deleted()) {
    // Unique client tags will have their version set to zero when they're
    // deleted.  The usual version comparison logic won't be able to detect
    // reflections of these items.  Instead, we assume any received tombstones
    // are reflections.  That should be correct most of the time.
    return false;
  }

  return existing_version < update.version();
}

// In the event that IDs match, but tags differ AttemptReuniteClient tag
// will have refused to unify the update.
// We should not attempt to apply it at all since it violates consistency
// rules.
VerifyResult VerifyTagConsistency(
    const sync_pb::SyncEntity& entry,
    const syncable::ModelNeutralMutableEntry& same_id) {
  if (entry.has_client_defined_unique_tag() &&
      entry.client_defined_unique_tag() != same_id.GetUniqueClientTag()) {
    return VERIFY_FAIL;
  }
  return VERIFY_UNDECIDED;
}

// Checks whether or not an update is fit for processing.
//
// The answer may be "no" if the update appears invalid, or it's not releveant
// (ie. a delete for an item we've never heard of), or other reasons.
VerifyResult VerifyUpdate(syncable::ModelNeutralWriteTransaction* trans,
                          const sync_pb::SyncEntity& entry,
                          ModelType requested_type) {
  syncable::Id id = SyncableIdFromProto(entry.id_string());
  VerifyResult result = VERIFY_FAIL;

  const bool deleted = entry.has_deleted() && entry.deleted();
  const bool is_directory = IsFolder(entry);
  const ModelType model_type = GetModelType(entry);

  if (!id.ServerKnows()) {
    LOG(ERROR) << "Illegal negative id in received updates";
    return result;
  }
  {
    const std::string name = SyncerProtoUtil::NameFromSyncEntity(entry);
    if (name.empty() && !deleted) {
      LOG(ERROR) << "Zero length name in non-deleted update";
      return result;
    }
  }

  syncable::ModelNeutralMutableEntry same_id(trans, GET_BY_ID, id);
  result = VerifyNewEntry(entry, &same_id, deleted);

  ModelType placement_type =
      !deleted ? GetModelType(entry) : same_id.good() ? same_id.GetModelType()
                                                      : UNSPECIFIED;

  if (VERIFY_UNDECIDED == result) {
    result = VerifyTagConsistency(entry, same_id);
  }

  if (VERIFY_UNDECIDED == result) {
    if (deleted) {
      // For deletes the server could send tombostones for items that
      // the client did not request. If so ignore those items.
      if (IsRealDataType(placement_type) && requested_type != placement_type) {
        result = VERIFY_SKIP;
      } else {
        result = VERIFY_SUCCESS;
      }
    }
  }

  // If we have an existing entry, we check here for updates that break
  // consistency rules.
  if (VERIFY_UNDECIDED == result) {
    result = VerifyUpdateConsistency(trans, entry, deleted, is_directory,
                                     model_type, &same_id);
  }

  if (VERIFY_UNDECIDED == result)
    result = VERIFY_SUCCESS;  // No news is good news.

  return result;  // This might be VERIFY_SUCCESS as well
}

// Returns true if the entry is still ok to process.
bool ReverifyEntry(syncable::ModelNeutralWriteTransaction* trans,
                   const sync_pb::SyncEntity& entry,
                   syncable::ModelNeutralMutableEntry* same_id) {
  const bool deleted = entry.has_deleted() && entry.deleted();
  const bool is_directory = IsFolder(entry);
  const ModelType model_type = GetModelType(entry);

  return VERIFY_SUCCESS == VerifyUpdateConsistency(trans, entry, deleted,
                                                   is_directory, model_type,
                                                   same_id);
}

// Process a single update. Will avoid touching global state.
//
// If the update passes a series of checks, this function will copy
// the SyncEntity's data into the SERVER side of the syncable::Directory.
void ProcessUpdate(const sync_pb::SyncEntity& update,
                   const Cryptographer* cryptographer,
                   syncable::ModelNeutralWriteTransaction* const trans) {
  const syncable::Id& server_id = SyncableIdFromProto(update.id_string());
  const std::string name = SyncerProtoUtil::NameFromSyncEntity(update);

  // Look to see if there's a local item that should recieve this update,
  // maybe due to a duplicate client tag or a lost commit response.
  syncable::Id local_id = FindLocalIdToUpdate(trans, update);

  // FindLocalEntryToUpdate has veto power.
  if (local_id.IsNull()) {
    return;  // The entry has become irrelevant.
  }

  CreateNewEntry(trans, local_id);

  // We take a two step approach. First we store the entries data in the
  // server fields of a local entry and then move the data to the local fields
  syncable::ModelNeutralMutableEntry target_entry(trans, GET_BY_ID, local_id);

  // We need to run the Verify checks again; the world could have changed
  // since we last verified.
  if (!ReverifyEntry(trans, update, &target_entry)) {
    return;  // The entry has become irrelevant.
  }

  // If we're repurposing an existing local entry with a new server ID,
  // change the ID now, after we're sure that the update can succeed.
  if (local_id != server_id) {
    DCHECK(!update.deleted());
    ChangeEntryIDAndUpdateChildren(trans, &target_entry, server_id);
    // When IDs change, versions become irrelevant.  Forcing BASE_VERSION
    // to zero would ensure that this update gets applied, but would indicate
    // creation or undeletion if it were committed that way.  Instead, prefer
    // forcing BASE_VERSION to entry.version() while also forcing
    // IS_UNAPPLIED_UPDATE to true.  If the item is UNSYNCED, it's committable
    // from the new state; it may commit before the conflict resolver gets
    // a crack at it.
    if (target_entry.GetIsUnsynced() || target_entry.GetBaseVersion() > 0) {
      // If either of these conditions are met, then we can expect valid client
      // fields for this entry.  When BASE_VERSION is positive, consistency is
      // enforced on the client fields at update-application time.  Otherwise,
      // we leave the BASE_VERSION field alone; it'll get updated the first time
      // we successfully apply this update.
      target_entry.PutBaseVersion(update.version());
    }
    // Force application of this update, no matter what.
    target_entry.PutIsUnappliedUpdate(true);
  }

  // If this is a newly received undecryptable update, and the only thing that
  // has changed are the specifics, store the original decryptable specifics,
  // (on which any current or future local changes are based) before we
  // overwrite SERVER_SPECIFICS.
  // MTIME, CTIME, and NON_UNIQUE_NAME are not enforced.

  bool position_matches = false;
  if (target_entry.ShouldMaintainPosition() && !update.deleted()) {
    std::string update_tag = GetUniqueBookmarkTagFromUpdate(update);
    if (UniquePosition::IsValidSuffix(update_tag)) {
      position_matches = GetUpdatePosition(update, update_tag)
                             .Equals(target_entry.GetServerUniquePosition());
    } else {
      NOTREACHED();
    }
  } else {
    // If this item doesn't care about positions, then set this flag to true.
    position_matches = true;
  }

  if (!update.deleted() && !target_entry.GetServerIsDel() &&
      (SyncableIdFromProto(update.parent_id_string()) ==
       target_entry.GetServerParentId()) &&
      position_matches && update.has_specifics() &&
      update.specifics().has_encrypted() &&
      !cryptographer->CanDecrypt(update.specifics().encrypted())) {
    sync_pb::EntitySpecifics prev_specifics = target_entry.GetServerSpecifics();
    // We only store the old specifics if they were decryptable and applied and
    // there is no BASE_SERVER_SPECIFICS already. Else do nothing.
    if (!target_entry.GetIsUnappliedUpdate() &&
        !IsRealDataType(
            GetModelTypeFromSpecifics(target_entry.GetBaseServerSpecifics())) &&
        (!prev_specifics.has_encrypted() ||
         cryptographer->CanDecrypt(prev_specifics.encrypted()))) {
      DVLOG(2) << "Storing previous server specifcs: "
               << prev_specifics.SerializeAsString();
      target_entry.PutBaseServerSpecifics(prev_specifics);
    }
  } else if (IsRealDataType(GetModelTypeFromSpecifics(
                 target_entry.GetBaseServerSpecifics()))) {
    // We have a BASE_SERVER_SPECIFICS, but a subsequent non-specifics-only
    // change arrived. As a result, we can't use the specifics alone to detect
    // changes, so we clear BASE_SERVER_SPECIFICS.
    target_entry.PutBaseServerSpecifics(sync_pb::EntitySpecifics());
  }

  UpdateServerFieldsFromUpdate(&target_entry, update, name);

  return;
}

}  // namespace

void ProcessDownloadedUpdates(syncable::Directory* dir,
                              syncable::ModelNeutralWriteTransaction* trans,
                              ModelType type,
                              const SyncEntityList& applicable_updates,
                              bool is_initial_sync,
                              StatusController* status,
                              UpdateCounters* counters) {
  for (const auto* update : applicable_updates) {
    DCHECK_EQ(type, GetModelType(*update));
    if (!UpdateContainsNewVersion(trans, *update)) {
      status->increment_num_reflected_updates_downloaded_by(1);
      counters->num_non_initial_reflected_updates_received++;
    }
    if (update->deleted()) {
      status->increment_num_tombstone_updates_downloaded_by(1);
      if (!is_initial_sync)
        counters->num_non_initial_tombstone_updates_received++;
    }
    VerifyResult verify_result = VerifyUpdate(trans, *update, type);
    if (verify_result != VERIFY_SUCCESS && verify_result != VERIFY_UNDELETE)
      continue;
    ProcessUpdate(*update, dir->GetCryptographer(trans), trans);
  }
}

void ExpireEntriesByVersion(syncable::Directory* dir,
                            syncable::ModelNeutralWriteTransaction* trans,
                            ModelType type,
                            int64_t version_watermark) {
  syncable::Directory::Metahandles handles;
  dir->GetMetaHandlesOfType(trans, type, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    syncable::ModelNeutralMutableEntry entry(trans, syncable::GET_BY_HANDLE,
                                             handles[i]);
    if (!entry.good() || !entry.GetId().ServerKnows() ||
        entry.GetUniqueServerTag() == ModelTypeToRootTag(type) ||
        entry.GetIsUnappliedUpdate() || entry.GetIsUnsynced() ||
        entry.GetIsDel() || entry.GetServerIsDel() ||
        entry.GetBaseVersion() >= version_watermark) {
      continue;
    }

    // Mark entry as unapplied update first to ensure journaling the deletion.
    entry.PutIsUnappliedUpdate(true);
    // Mark entry as deleted by server.
    entry.PutServerIsDel(true);
    entry.PutServerVersion(version_watermark);
  }
}

void ExpireEntriesByAge(syncable::Directory* dir,
                        syncable::ModelNeutralWriteTransaction* trans,
                        ModelType type,
                        int32_t age_watermark_in_days) {
  syncable::Directory::Metahandles handles;
  base::Time to_be_expired =
      base::Time::Now() - base::TimeDelta::FromDays(age_watermark_in_days);
  dir->GetMetaHandlesOfType(trans, type, &handles);
  for (size_t i = 0; i < handles.size(); ++i) {
    syncable::ModelNeutralMutableEntry entry(trans, syncable::GET_BY_HANDLE,
                                             handles[i]);
    if (!entry.good() || !entry.GetId().ServerKnows() ||
        entry.GetUniqueServerTag() == ModelTypeToRootTag(type) ||
        entry.GetIsUnappliedUpdate() || entry.GetIsUnsynced() ||
        entry.GetIsDel() || entry.GetServerIsDel() ||
        entry.GetMtime() > to_be_expired) {
      continue;
    }

    // Mark entry as unapplied update first to ensure journaling the deletion.
    entry.PutIsUnappliedUpdate(true);
    // Mark entry as deleted by server.
    entry.PutServerIsDel(true);
  }
}

}  // namespace syncer
