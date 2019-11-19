// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/syncer_util.h"

#include <algorithm>

#include "base/base64.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine_impl/conflict_resolver.h"
#include "components/sync/engine_impl/syncer_proto_util.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/syncable_changes_version.h"
#include "components/sync/syncable/syncable_model_neutral_write_transaction.h"
#include "components/sync/syncable/syncable_proto_util.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_util.h"
#include "components/sync/syncable/syncable_write_transaction.h"

namespace syncer {

using syncable::CHANGES_VERSION;
using syncable::Directory;
using syncable::Entry;
using syncable::GET_BY_HANDLE;
using syncable::GET_BY_ID;
using syncable::ID;
using syncable::Id;

syncable::Id FindLocalIdToUpdate(syncable::BaseTransaction* trans,
                                 const sync_pb::SyncEntity& update) {
  // Expected entry points of this function:
  // SyncEntity has NOT been applied to SERVER fields.
  // SyncEntity has NOT been applied to LOCAL fields.
  // DB has not yet been modified, no entries created for this update.

  const std::string& client_id = trans->directory()->cache_guid();
  const syncable::Id& update_id = SyncableIdFromProto(update.id_string());

  if (update.has_client_defined_unique_tag() &&
      !update.client_defined_unique_tag().empty()) {
    // When a server sends down a client tag, the following cases can occur:
    // 1) Client has entry for tag already, ID is server style, matches
    // 2) Client has entry for tag already, ID is server, doesn't match.
    // 3) Client has entry for tag already, ID is local, (never matches)
    // 4) Client has no entry for tag

    // Case 1, we don't have to do anything since the update will
    // work just fine. Update will end up in the proper entry, via ID lookup.
    // Case 2 - Happens very rarely due to lax enforcement of client tags
    // on the server, if two clients commit the same tag at the same time.
    // When this happens, we pick the lexically-least ID and ignore all other
    // items.
    // Case 3 - We need to replace the local ID with the server ID so that
    // this update gets targeted at the correct local entry; we expect conflict
    // resolution to occur.
    // Case 4 - Perfect. Same as case 1.

    syncable::Entry local_entry(trans, syncable::GET_BY_CLIENT_TAG,
                                update.client_defined_unique_tag());

    // The SyncAPI equivalent of this function will return !good if IS_DEL.
    // The syncable version will return good even if IS_DEL.
    // TODO(chron): Unit test the case with IS_DEL and make sure.
    if (local_entry.good()) {
      if (local_entry.GetId().ServerKnows()) {
        if (local_entry.GetId() != update_id) {
          // Case 2.
          LOG(WARNING) << "Duplicated client tag.";
          if (local_entry.GetId() < update_id) {
            // Signal an error; drop this update on the floor.  Note that
            // we don't server delete the item, because we don't allow it to
            // exist locally at all.  So the item will remain orphaned on
            // the server, and we won't pay attention to it.
            return syncable::Id();
          }
        }
        // Target this change to the existing local entry; later,
        // we'll change the ID of the local entry to update_id
        // if needed.
        return local_entry.GetId();
      } else {
        // Case 3: We have a local entry with the same client tag.
        // We should change the ID of the local entry to the server entry.
        // This will result in an server ID with base version == 0, but that's
        // a legal state for an item with a client tag.  By changing the ID,
        // update will now be applied to local_entry.
        DCHECK(0 == local_entry.GetBaseVersion() ||
               CHANGES_VERSION == local_entry.GetBaseVersion());
        return local_entry.GetId();
      }
    }
  } else if (update.has_originator_cache_guid() &&
             update.originator_cache_guid() == client_id) {
    // If a commit succeeds, but the response does not come back fast enough
    // then the syncer might assume that it was never committed.
    // The server will track the client that sent up the original commit and
    // return this in a get updates response. When this matches a local
    // uncommitted item, we must mutate our local item and version to pick up
    // the committed version of the same item whose commit response was lost.
    // There is however still a race condition if the server has not
    // completed the commit by the time the syncer tries to get updates
    // again. To mitigate this, we need to have the server time out in
    // a reasonable span, our commit batches have to be small enough
    // to process within our HTTP response "assumed alive" time.

    // We need to check if we have an entry that didn't get its server
    // id updated correctly. The server sends down a client ID
    // and a local (negative) id. If we have a entry by that
    // description, we should update the ID and version to the
    // server side ones to avoid multiple copies of the same thing.

    syncable::Id client_item_id = syncable::Id::CreateFromClientString(
        update.originator_client_item_id());
    DCHECK(!client_item_id.ServerKnows());
    syncable::Entry local_entry(trans, GET_BY_ID, client_item_id);

    // If it exists, then our local client lost a commit response.  Use
    // the local entry.
    if (local_entry.good() && !local_entry.GetIsDel()) {
      int64_t old_version = local_entry.GetBaseVersion();
      int64_t new_version = update.version();
      DCHECK_LE(old_version, 0);
      DCHECK_GT(new_version, 0);
      // Otherwise setting the base version could cause a consistency failure.
      // An entry should never be version 0 and SYNCED.
      DCHECK(local_entry.GetIsUnsynced());

      // Just a quick sanity check.
      DCHECK(!local_entry.GetId().ServerKnows());

      DVLOG(1) << "Reuniting lost commit response IDs. server id: " << update_id
               << " local id: " << local_entry.GetId()
               << " new version: " << new_version;

      return local_entry.GetId();
    }
  } else if (update.has_server_defined_unique_tag() &&
             !update.server_defined_unique_tag().empty()) {
    // The client creates type root folders with a local ID on demand when a
    // progress marker for the given type is initially set.
    // The server might also attempt to send a type root folder for the same
    // type (during the transition period until support for root folders is
    // removed for new client versions).
    // TODO(stanisc): crbug.com/438313: remove this once the transition to
    // implicit root folders on the server is done.
    syncable::Entry local_entry(trans, syncable::GET_BY_SERVER_TAG,
                                update.server_defined_unique_tag());
    if (local_entry.good() && !local_entry.GetId().ServerKnows()) {
      DCHECK(local_entry.GetId() != update_id);
      return local_entry.GetId();
    }
  }

  // Fallback: target an entry having the server ID, creating one if needed.
  return update_id;
}

UpdateAttemptResponse AttemptToUpdateEntry(
    syncable::WriteTransaction* const trans,
    syncable::MutableEntry* const entry,
    const Cryptographer* cryptographer) {
  DCHECK(entry->good());
  if (!entry->GetIsUnappliedUpdate())
    return SUCCESS;  // No work to do.
  syncable::Id id = entry->GetId();
  const sync_pb::EntitySpecifics& specifics = entry->GetServerSpecifics();
  ModelType type = GetModelTypeFromSpecifics(specifics);

  // Only apply updates that we can decrypt. If we can't decrypt the update, it
  // is likely because the passphrase has not arrived yet. Because the
  // passphrase may not arrive within this GetUpdates, we can't just return
  // conflict, else we try to perform normal conflict resolution prematurely or
  // the syncer may get stuck. As such, we return CONFLICT_ENCRYPTION, which is
  // treated as an unresolvable conflict. See the description in syncer_types.h.
  // This prevents any unsynced changes from commiting and postpones conflict
  // resolution until all data can be decrypted.
  if (specifics.has_encrypted() &&
      !cryptographer->CanDecrypt(specifics.encrypted())) {
    // We can't decrypt this node yet.
    DVLOG(1) << "Received an undecryptable "
             << ModelTypeToString(entry->GetServerModelType())
             << " update, returning conflict_encryption.";
    return CONFLICT_ENCRYPTION;
  } else if (specifics.has_password() && entry->GetUniqueServerTag().empty()) {
    // Passwords use their own legacy encryption scheme.
    const sync_pb::PasswordSpecifics& password = specifics.password();
    if (!cryptographer->CanDecrypt(password.encrypted())) {
      DVLOG(1) << "Received an undecryptable password update, returning "
               << "conflict_encryption.";
      return CONFLICT_ENCRYPTION;
    }
  }

  if (!entry->GetServerIsDel()) {
    syncable::Id new_parent = entry->GetServerParentId();
    if (!new_parent.IsNull()) {
      // Perform this step only if the parent is specified.
      // The unset parent means that the implicit type root would be used.
      Entry parent(trans, GET_BY_ID, new_parent);
      // A note on non-directory parents:
      // We catch most unfixable tree invariant errors at update receipt time,
      // however we deal with this case here because we may receive the child
      // first then the illegal parent. Instead of dealing with it twice in
      // different ways we deal with it once here to reduce the amount of code
      // and potential errors.
      if (!parent.good() || parent.GetIsDel() || !parent.GetIsDir()) {
        DVLOG(1) << "Entry has bad parent, returning conflict_hierarchy.";
        return CONFLICT_HIERARCHY;
      }
      if (entry->GetParentId() != new_parent) {
        if (!entry->GetIsDel() && !IsLegalNewParent(trans, id, new_parent)) {
          DVLOG(1) << "Not updating item " << id
                   << ", illegal new parent (would cause loop).";
          return CONFLICT_HIERARCHY;
        }
      }
    } else {
      // new_parent is unset.
      DCHECK(IsTypeWithClientGeneratedRoot(type));
    }
  } else if (entry->GetIsDir()) {
    Directory::Metahandles handles;
    trans->directory()->GetChildHandlesById(trans, id, &handles);
    if (!handles.empty()) {
      // If we have still-existing children, then we need to deal with
      // them before we can process this change.
      DVLOG(1) << "Not deleting directory; it's not empty " << *entry;
      return CONFLICT_HIERARCHY;
    }
  }

  if (entry->GetIsUnsynced()) {
    DVLOG(1) << "Skipping update, returning conflict for: " << id
             << " ; it's unsynced.";
    return CONFLICT_SIMPLE;
  }

  if (specifics.has_encrypted()) {
    DVLOG(2) << "Received a decryptable "
             << ModelTypeToString(entry->GetServerModelType())
             << " update, applying normally.";
  } else {
    DVLOG(2) << "Received an unencrypted "
             << ModelTypeToString(entry->GetServerModelType())
             << " update, applying normally.";
  }

  UpdateLocalDataFromServerData(trans, entry);

  return SUCCESS;
}

std::string GetUniqueBookmarkTagFromUpdate(const sync_pb::SyncEntity& update) {
  if (!update.has_originator_cache_guid() ||
      !update.has_originator_client_item_id()) {
    LOG(ERROR) << "Update is missing requirements for bookmark position."
               << " This is a server bug.";
    return UniquePosition::RandomSuffix();
  }

  return GenerateSyncableBookmarkHash(update.originator_cache_guid(),
                                      update.originator_client_item_id());
}

UniquePosition GetUpdatePosition(const sync_pb::SyncEntity& update,
                                 const std::string& suffix) {
  DCHECK(UniquePosition::IsValidSuffix(suffix));
  if (!(SyncerProtoUtil::ShouldMaintainPosition(update))) {
    return UniquePosition::CreateInvalid();
  } else if (update.has_unique_position()) {
    UniquePosition proto_position =
        UniquePosition::FromProto(update.unique_position());
    if (proto_position.IsValid()) {
      return proto_position;
    }
  }

  // Now, there are two cases hit here.
  // 1. Did not receive unique_position for this update.
  // 2. Received unique_position, but it is invalid(ex. empty).
  // And we will create a valid position for this two cases.
  if (update.has_position_in_parent()) {
    return UniquePosition::FromInt64(update.position_in_parent(), suffix);
  } else {
    LOG(ERROR) << "No position information in update. This is a server bug.";
    return UniquePosition::FromInt64(0, suffix);
  }
}

namespace {

void UpdateBookmarkPositioning(
    const sync_pb::SyncEntity& update,
    syncable::ModelNeutralMutableEntry* local_entry) {
  // Update our unique bookmark tag.  In many cases this will be identical to
  // the tag we already have.  However, clients that have recently upgraded to
  // versions that support unique positions will have incorrect tags.  See the
  // v86 migration logic in directory_backing_store.cc for more information.
  //
  // Both the old and new values are unique to this element.  Applying this
  // update will not risk the creation of conflicting unique tags.
  std::string bookmark_tag = GetUniqueBookmarkTagFromUpdate(update);
  if (UniquePosition::IsValidSuffix(bookmark_tag)) {
    local_entry->PutUniqueBookmarkTag(bookmark_tag);
  }

  // Update our position.
  UniquePosition update_pos =
      GetUpdatePosition(update, local_entry->GetUniqueBookmarkTag());
  if (update_pos.IsValid()) {
    local_entry->PutServerUniquePosition(update_pos);
  }
}

}  // namespace

void UpdateServerFieldsFromUpdate(syncable::ModelNeutralMutableEntry* target,
                                  const sync_pb::SyncEntity& update,
                                  const std::string& name) {
  if (update.deleted()) {
    if (target->GetServerIsDel()) {
      // If we already think the item is server-deleted, we're done.
      // Skipping these cases prevents our committed deletions from coming
      // back and overriding subsequent undeletions.  For non-deleted items,
      // the version number check has a similar effect.
      return;
    }
    // Mark entry as unapplied update first to ensure journaling the deletion.
    target->PutIsUnappliedUpdate(true);
    // The server returns very lightweight replies for deletions, so we don't
    // clobber a bunch of fields on delete.
    target->PutServerIsDel(true);
    if (!target->GetUniqueClientTag().empty()) {
      // Items identified by the client unique tag are undeletable; when
      // they're deleted, they go back to version 0.
      target->PutServerVersion(0);
    } else {
      // Otherwise, fake a server version by bumping the local number.
      target->PutServerVersion(
          std::max(target->GetServerVersion(), target->GetBaseVersion()) + 1);
    }
    return;
  }

  DCHECK_EQ(target->GetId(), SyncableIdFromProto(update.id_string()))
      << "ID Changing not supported here";
  if (SyncerProtoUtil::ShouldMaintainHierarchy(update)) {
    target->PutServerParentId(SyncableIdFromProto(update.parent_id_string()));
  } else {
    target->PutServerParentId(syncable::Id());
  }
  target->PutServerNonUniqueName(name);
  target->PutServerVersion(update.version());
  target->PutServerCtime(ProtoTimeToTime(update.ctime()));
  target->PutServerMtime(ProtoTimeToTime(update.mtime()));
  target->PutServerIsDir(IsFolder(update));
  if (update.has_server_defined_unique_tag()) {
    const std::string& tag = update.server_defined_unique_tag();
    target->PutUniqueServerTag(tag);
  }
  if (update.has_client_defined_unique_tag()) {
    const std::string& tag = update.client_defined_unique_tag();
    target->PutUniqueClientTag(tag);
  }
  // Store the datatype-specific part as a protobuf.
  if (update.has_specifics()) {
    DCHECK_NE(GetModelType(update), UNSPECIFIED)
        << "Storing unrecognized datatype in sync database.";
    target->PutServerSpecifics(update.specifics());
  }
  if (SyncerProtoUtil::ShouldMaintainPosition(update)) {
    UpdateBookmarkPositioning(update, target);
  }

  // We only mark the entry as unapplied if its version is greater than the
  // local data. If we're processing the update that corresponds to one of our
  // commit we don't apply it as time differences may occur.
  if (update.version() > target->GetBaseVersion()) {
    target->PutIsUnappliedUpdate(true);
  }
  DCHECK(!update.deleted());
  target->PutServerIsDel(false);
}

// Creates a new Entry iff no Entry exists with the given id.
void CreateNewEntry(syncable::ModelNeutralWriteTransaction* trans,
                    const syncable::Id& id) {
  syncable::Entry entry(trans, GET_BY_ID, id);
  if (!entry.good()) {
    syncable::ModelNeutralMutableEntry new_entry(
        trans, syncable::CREATE_NEW_UPDATE_ITEM, id);
  }
}

// This function is called on an entry when we can update the user-facing data
// from the server data.
void UpdateLocalDataFromServerData(syncable::WriteTransaction* trans,
                                   syncable::MutableEntry* entry) {
  DCHECK(!entry->GetIsUnsynced());
  DCHECK(entry->GetIsUnappliedUpdate());

  DVLOG(2) << "Updating entry : " << *entry;
  // Start by setting the properties that determine the model_type.
  entry->PutSpecifics(entry->GetServerSpecifics());
  // Clear the previous server specifics now that we're applying successfully.
  entry->PutBaseServerSpecifics(sync_pb::EntitySpecifics());
  entry->PutIsDir(entry->GetServerIsDir());
  // This strange dance around the IS_DEL flag avoids problems when setting
  // the name.
  // TODO(chron): Is this still an issue? Unit test this codepath.
  if (entry->GetServerIsDel()) {
    entry->PutIsDel(true);
  } else {
    entry->PutNonUniqueName(entry->GetServerNonUniqueName());
    entry->PutParentId(entry->GetServerParentId());
    entry->PutUniquePosition(entry->GetServerUniquePosition());
    entry->PutIsDel(false);
  }

  entry->PutCtime(entry->GetServerCtime());
  entry->PutMtime(entry->GetServerMtime());
  entry->PutBaseVersion(entry->GetServerVersion());
  entry->PutIsDel(entry->GetServerIsDel());
  entry->PutIsUnappliedUpdate(false);
}

void MarkDeletedChildrenSynced(syncable::Directory* dir,
                               syncable::BaseWriteTransaction* trans,
                               std::set<syncable::Id>* deleted_folders) {
  // There's two options here.
  // 1. Scan deleted unsynced entries looking up their pre-delete tree for any
  //    of the deleted folders.
  // 2. Take each folder and do a tree walk of all entries underneath it.
  // #2 has a lower big O cost, but writing code to limit the time spent inside
  // the transaction during each step is simpler with 1. Changing this decision
  // may be sensible if this code shows up in profiling.
  if (deleted_folders->empty())
    return;
  Directory::Metahandles handles;
  dir->GetUnsyncedMetaHandles(trans, &handles);
  if (handles.empty())
    return;
  Directory::Metahandles::iterator it;
  for (it = handles.begin(); it != handles.end(); ++it) {
    syncable::ModelNeutralMutableEntry entry(trans, GET_BY_HANDLE, *it);
    if (!entry.GetIsUnsynced() || !entry.GetIsDel())
      continue;
    syncable::Id id = entry.GetParentId();
    while (id != trans->root_id()) {
      if (deleted_folders->find(id) != deleted_folders->end()) {
        // We've synced the deletion of this deleted entries parent.
        entry.PutIsUnsynced(false);
        break;
      }
      Entry parent(trans, GET_BY_ID, id);
      if (!parent.good() || !parent.GetIsDel())
        break;
      id = parent.GetParentId();
    }
  }
}

VerifyResult VerifyNewEntry(const sync_pb::SyncEntity& update,
                            syncable::Entry* target,
                            const bool deleted) {
  if (target->good()) {
    // Not a new update.
    return VERIFY_UNDECIDED;
  }
  if (deleted) {
    // Deletion of an item we've never seen can be ignored.
    return VERIFY_SKIP;
  }

  return VERIFY_SUCCESS;
}

// Assumes we have an existing entry; check here for updates that break
// consistency rules.
VerifyResult VerifyUpdateConsistency(
    syncable::ModelNeutralWriteTransaction* trans,
    const sync_pb::SyncEntity& update,
    const bool deleted,
    const bool is_directory,
    ModelType model_type,
    syncable::ModelNeutralMutableEntry* target) {
  DCHECK(target->good());
  const syncable::Id& update_id = SyncableIdFromProto(update.id_string());

  // If the update is a delete, we don't really need to worry at this stage.
  if (deleted)
    return VERIFY_SUCCESS;

  if (model_type == UNSPECIFIED) {
    // This update is to an item of a datatype we don't recognize. The server
    // shouldn't have sent it to us.  Throw it on the ground.
    return VERIFY_SKIP;
  }

  if (target->GetServerVersion() > 0) {
    // Then we've had an update for this entry before.
    if (is_directory != target->GetServerIsDir() ||
        model_type != target->GetServerModelType()) {
      if (target->GetIsDel()) {  // If we've deleted the item, we don't care.
        return VERIFY_SKIP;
      } else {
        LOG(ERROR) << "Server update doesn't agree with previous updates. ";
        LOG(ERROR) << " Entry: " << *target;
        LOG(ERROR) << " Update: "
                   << SyncerProtoUtil::SyncEntityDebugString(update);
        return VERIFY_FAIL;
      }
    }

    if (!deleted && (target->GetId() == update_id) &&
        (target->GetServerIsDel() ||
         (!target->GetIsUnsynced() && target->GetIsDel() &&
          target->GetBaseVersion() > 0))) {
      // An undelete. The latter case in the above condition is for
      // when the server does not give us an update following the
      // commit of a delete, before undeleting.
      // Undeletion is common for items that reuse the client-unique tag.
      VerifyResult result = VerifyUndelete(trans, update, target);
      if (VERIFY_UNDECIDED != result)
        return result;
    }
  }
  if (target->GetBaseVersion() > 0) {
    // We've committed this update in the past.
    if (is_directory != target->GetIsDir() ||
        model_type != target->GetModelType()) {
      LOG(ERROR) << "Server update doesn't agree with committed item. ";
      LOG(ERROR) << " Entry: " << *target;
      LOG(ERROR) << " Update: "
                 << SyncerProtoUtil::SyncEntityDebugString(update);
      return VERIFY_FAIL;
    }
    if (target->GetId() == update_id) {
      if (target->GetServerVersion() > update.version()) {
        LOG(WARNING) << "We've already seen a more recent version.";
        LOG(WARNING) << " Entry: " << *target;
        LOG(WARNING) << " Update: "
                     << SyncerProtoUtil::SyncEntityDebugString(update);
        return VERIFY_SKIP;
      }
    }
  }
  return VERIFY_SUCCESS;
}

// Assumes we have an existing entry; verify an update that seems to be
// expressing an 'undelete'
VerifyResult VerifyUndelete(syncable::ModelNeutralWriteTransaction* trans,
                            const sync_pb::SyncEntity& update,
                            syncable::ModelNeutralMutableEntry* target) {
  // TODO(nick): We hit this path for items deleted items that the server
  // tells us to re-create; only deleted items with positive base versions
  // will hit this path.  However, it's not clear how such an undeletion
  // would actually succeed on the server; in the protocol, a base
  // version of 0 is required to undelete an object.  This codepath
  // should be deprecated in favor of client-tag style undeletion
  // (where items go to version 0 when they're deleted), or else
  // removed entirely (if this type of undeletion is indeed impossible).
  DCHECK(target->good());
  DVLOG(1) << "Server update is attempting undelete. " << *target
           << "Update:" << SyncerProtoUtil::SyncEntityDebugString(update);
  // Move the old one aside and start over.  It's too tricky to get the old one
  // back into a state that would pass CheckTreeInvariants().
  if (target->GetIsDel()) {
    if (target->GetUniqueClientTag().empty())
      LOG(WARNING) << "Doing move-aside undeletion on client-tagged item.";
    target->PutId(trans->directory()->NextId());
    target->PutUniqueClientTag(std::string());
    target->PutBaseVersion(CHANGES_VERSION);
    target->PutServerVersion(0);
    return VERIFY_SUCCESS;
  }
  if (update.version() < target->GetServerVersion()) {
    LOG(WARNING) << "Update older than current server version for " << *target
                 << " Update:"
                 << SyncerProtoUtil::SyncEntityDebugString(update);
    return VERIFY_SUCCESS;  // Expected in new sync protocol.
  }
  return VERIFY_UNDECIDED;
}

}  // namespace syncer
