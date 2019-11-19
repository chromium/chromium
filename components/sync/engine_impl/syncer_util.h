// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_UTIL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_UTIL_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "components/sync/engine_impl/syncer.h"
#include "components/sync/engine_impl/syncer_types.h"
#include "components/sync/syncable/entry_kernel.h"
#include "components/sync/syncable/metahandle_set.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_id.h"

namespace sync_pb {
class SyncEntity;
}  // namespace sync_pb

namespace syncer {

namespace syncable {
class BaseTransaction;
class ModelNeutralWriteTransaction;
}  // namespace syncable

class Cryptographer;

// Utility functions manipulating syncable::Entries, intended for use by the
// syncer.

// If the server sent down a client-tagged entry, or an entry whose
// commit response was lost, it is necessary to update a local entry
// with an ID that doesn't match the ID of the update.  Here, we
// find the ID of such an entry, if it exists.  This function may
// determine that |server_entry| should be dropped; if so, it returns
// the null ID -- callers must handle this case.  When update application
// should proceed normally with a new local entry, this function will
// return server_entry.id(); the caller must create an entry with that
// ID.  This function does not alter the database.
syncable::Id FindLocalIdToUpdate(syncable::BaseTransaction* trans,
                                 const sync_pb::SyncEntity& server_entry);

UpdateAttemptResponse AttemptToUpdateEntry(
    syncable::WriteTransaction* const trans,
    syncable::MutableEntry* const entry,
    const Cryptographer* cryptographer);

// Returns the most accurate position information available in this update.  It
// prefers to use the unique_position() field, but will fall back to using the
// int64_t-based position_in_parent if necessary.
//
// The suffix parameter is the unique bookmark tag for the item being updated.
//
// Will return an invalid position if no valid position can be constructed, or
// if this type does not support positioning.
UniquePosition GetUpdatePosition(const sync_pb::SyncEntity& update,
                                 const std::string& suffix);

// Fetch the cache_guid and item_id-based unique bookmark tag from an update.
// Will return an empty string if someting unexpected happens.
std::string GetUniqueBookmarkTagFromUpdate(const sync_pb::SyncEntity& update);

// Pass in name to avoid redundant UTF8 conversion.
void UpdateServerFieldsFromUpdate(
    syncable::ModelNeutralMutableEntry* local_entry,
    const sync_pb::SyncEntity& server_entry,
    const std::string& name);

// Creates a new Entry iff no Entry exists with the given id.
void CreateNewEntry(syncable::ModelNeutralWriteTransaction* trans,
                    const syncable::Id& id);

// This function is called on an entry when we can update the user-facing data
// from the server data.
void UpdateLocalDataFromServerData(syncable::WriteTransaction* trans,
                                   syncable::MutableEntry* entry);

VerifyResult VerifyNewEntry(const sync_pb::SyncEntity& update,
                            syncable::Entry* target,
                            const bool deleted);

// Assumes we have an existing entry; check here for updates that break
// consistency rules.
VerifyResult VerifyUpdateConsistency(
    syncable::ModelNeutralWriteTransaction* trans,
    const sync_pb::SyncEntity& update,
    const bool deleted,
    const bool is_directory,
    ModelType model_type,
    syncable::ModelNeutralMutableEntry* target);

// Assumes we have an existing entry; verify an update that seems to be
// expressing an 'undelete'
VerifyResult VerifyUndelete(syncable::ModelNeutralWriteTransaction* trans,
                            const sync_pb::SyncEntity& update,
                            syncable::ModelNeutralMutableEntry* target);

void MarkDeletedChildrenSynced(syncable::Directory* dir,
                               syncable::BaseWriteTransaction* trans,
                               std::set<syncable::Id>* deleted_folders);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_SYNCER_UTIL_H_
