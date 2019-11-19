// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/conflict_resolver.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "components/sync/engine/cycle/update_counters.h"
#include "components/sync/engine_impl/conflict_util.h"
#include "components/sync/engine_impl/cycle/status_controller.h"
#include "components/sync/engine_impl/syncer_util.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/mutable_entry.h"
#include "components/sync/syncable/syncable_write_transaction.h"

using std::set;

namespace syncer {

namespace {

using syncable::Directory;
using syncable::Entry;
using syncable::Id;
using syncable::MutableEntry;

bool CanDecryptUsingDefaultKey(const Cryptographer& cryptographer,
                               const sync_pb::EncryptedData& encrypted) {
  return !encrypted.key_name().empty() &&
         encrypted.key_name() == cryptographer.GetDefaultEncryptionKeyName();
}

}  // namespace

ConflictResolver::ConflictResolver() {}

ConflictResolver::~ConflictResolver() {}

void ConflictResolver::ProcessSimpleConflict(syncable::WriteTransaction* trans,
                                             const Id& id,
                                             const Cryptographer* cryptographer,
                                             StatusController* status,
                                             UpdateCounters* counters) {
  MutableEntry entry(trans, syncable::GET_BY_ID, id);
  // Must be good as the entry won't have been cleaned up.
  DCHECK(entry.good());

  // This function can only resolve simple conflicts.  Simple conflicts have
  // both IS_UNSYNCED and IS_UNAPPLIED_UDPATE set.
  if (!entry.GetIsUnappliedUpdate() || !entry.GetIsUnsynced()) {
    // This is very unusual, but it can happen in tests.  We may be able to
    // assert NOTREACHED() here when those tests are updated.
    return;
  }

  if (entry.GetIsDel() && entry.GetServerIsDel()) {
    // we've both deleted it, so lets just drop the need to commit/update this
    // entry.
    entry.PutIsUnsynced(false);
    entry.PutIsUnappliedUpdate(false);
    // we've made changes, but they won't help syncing progress.
    // METRIC simple conflict resolved by merge.
    return;
  }

  // This logic determines "client wins" vs. "server wins" strategy picking.
  // By the time we get to this point, we rely on the following to be true:
  // a) We can decrypt both the local and server data (else we'd be in
  //    conflict encryption and not attempting to resolve).
  // b) All unsynced changes have been re-encrypted with the default key (
  //    occurs either in AttemptToUpdateEntry, SetEncryptionPassphrase,
  //    SetDecryptionPassphrase, or RefreshEncryption).
  // c) Base_server_specifics having a valid datatype means that we received
  //    an undecryptable update that only changed specifics, and since then have
  //    not received any further non-specifics-only or decryptable updates.
  // d) If the server_specifics match specifics, server_specifics are
  //    encrypted with the default key, and all other visible properties match,
  //    then we can safely ignore the local changes as redundant.
  // e) Otherwise if the base_server_specifics match the server_specifics, no
  //    functional change must have been made server-side (else
  //    base_server_specifics would have been cleared), and we can therefore
  //    safely ignore the server changes as redundant.
  // f) Otherwise, it's in general safer to ignore local changes, with the
  //    exception of deletion conflicts (choose to undelete) and conflicts
  //    where the non_unique_name or parent don't match.
  // e) Except for the case of extensions and apps, where we want uninstalls to
  //    win over local modifications to avoid "back from the dead" reinstalls.
  if (!entry.GetServerIsDel()) {
    // TODO(nick): The current logic is arbitrary; instead, it ought to be made
    // consistent with the ModelAssociator behavior for a datatype.  It would
    // be nice if we could route this back to ModelAssociator code to pick one
    // of three options: CLIENT, SERVER, or MERGE.  Some datatypes (autofill)
    // are easily mergeable.
    // See http://crbug.com/77339.
    bool name_matches =
        entry.GetNonUniqueName() == entry.GetServerNonUniqueName();
    // The parent is implicit type root folder or the parent ID matches.
    bool parent_matches = entry.GetServerParentId().IsNull() ||
                          entry.GetParentId() == entry.GetServerParentId();
    bool entry_deleted = entry.GetIsDel();
    // The position check might fail spuriously if one of the positions was
    // based on a legacy random suffix, rather than a deterministic one based on
    // originator_cache_guid and originator_item_id.  If an item is being
    // modified regularly, it shouldn't take long for the suffix and position to
    // be updated, so such false failures shouldn't be a problem for long.
    //
    // Lucky for us, it's OK to be wrong here.  The position_matches check is
    // allowed to return false negatives, as long as it returns no false
    // positives.
    bool position_matches =
        parent_matches &&
        entry.GetServerUniquePosition().Equals(entry.GetUniquePosition());
    const sync_pb::EntitySpecifics& specifics = entry.GetSpecifics();
    const sync_pb::EntitySpecifics& server_specifics =
        entry.GetServerSpecifics();
    const sync_pb::EntitySpecifics& base_server_specifics =
        entry.GetBaseServerSpecifics();
    std::string decrypted_specifics, decrypted_server_specifics;
    bool specifics_match = false;
    bool server_encrypted_with_default_key = false;
    if (specifics.has_encrypted()) {
      DCHECK(CanDecryptUsingDefaultKey(*cryptographer, specifics.encrypted()));
      // TODO(crbug.com/908391): what if the decryption below fails?
      cryptographer->DecryptToString(specifics.encrypted(),
                                     &decrypted_specifics);
    } else {
      decrypted_specifics = specifics.SerializeAsString();
    }
    if (server_specifics.has_encrypted()) {
      server_encrypted_with_default_key = CanDecryptUsingDefaultKey(
          *cryptographer, server_specifics.encrypted());
      // TODO(crbug.com/908391): what if the decryption below fails?
      cryptographer->DecryptToString(server_specifics.encrypted(),
                                     &decrypted_server_specifics);
    } else {
      decrypted_server_specifics = server_specifics.SerializeAsString();
    }
    if (decrypted_server_specifics == decrypted_specifics &&
        server_encrypted_with_default_key == specifics.has_encrypted()) {
      specifics_match = true;
    }
    bool base_server_specifics_match = false;
    if (server_specifics.has_encrypted() &&
        IsRealDataType(GetModelTypeFromSpecifics(base_server_specifics))) {
      std::string decrypted_base_server_specifics;
      if (!base_server_specifics.has_encrypted()) {
        decrypted_base_server_specifics =
            base_server_specifics.SerializeAsString();
      } else {
        // TODO(crbug.com/908391): what if the decryption below fails?
        cryptographer->DecryptToString(base_server_specifics.encrypted(),
                                       &decrypted_base_server_specifics);
      }
      if (decrypted_server_specifics == decrypted_base_server_specifics)
        base_server_specifics_match = true;
    }

    if (!entry_deleted && name_matches && parent_matches && specifics_match &&
        position_matches) {
      DVLOG(1) << "Resolving simple conflict, everything matches, ignoring "
               << "changes for: " << entry;
      conflict_util::IgnoreConflict(&entry);
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict", CHANGES_MATCH,
                                CONFLICT_RESOLUTION_SIZE);
    } else if (base_server_specifics_match) {
      DVLOG(1) << "Resolving simple conflict, ignoring server encryption "
               << " changes for: " << entry;
      status->increment_num_server_overwrites();
      counters->num_server_overwrites++;
      conflict_util::OverwriteServerChanges(&entry);
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict", IGNORE_ENCRYPTION,
                                CONFLICT_RESOLUTION_SIZE);
    } else if (entry_deleted || !name_matches || !parent_matches) {
      // NOTE: The update application logic assumes that conflict resolution
      // will never result in changes to the local hierarchy.  The entry_deleted
      // and !parent_matches cases here are critical to maintaining that
      // assumption.
      conflict_util::OverwriteServerChanges(&entry);
      status->increment_num_server_overwrites();
      counters->num_server_overwrites++;
      DVLOG(1) << "Resolving simple conflict, overwriting server changes "
               << "for: " << entry;
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict", OVERWRITE_SERVER,
                                CONFLICT_RESOLUTION_SIZE);
      // TODO(crbug.com/890746): It seems like local deletion can override a
      // remote update, which goes against the usual spirit of undeletion-wins,
      // and differs from the USS logic.
    } else {
      DVLOG(1) << "Resolving simple conflict, ignoring local changes for: "
               << entry;
      conflict_util::IgnoreLocalChanges(&entry);
      status->increment_num_local_overwrites();
      counters->num_local_overwrites++;
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict", OVERWRITE_LOCAL,
                                CONFLICT_RESOLUTION_SIZE);
    }
    // Now that we've resolved the conflict, clear the prev server
    // specifics.
    entry.PutBaseServerSpecifics(sync_pb::EntitySpecifics());
  } else {  // SERVER_IS_DEL is true
    ModelType type = entry.GetModelType();
    if (type == EXTENSIONS || type == APPS) {
      // Ignore local changes for extensions/apps when server had a delete, to
      // avoid unwanted reinstall of an uninstalled extension.
      DVLOG(1) << "Resolving simple conflict, ignoring local changes for "
               << "extension/app: " << entry;
      conflict_util::IgnoreLocalChanges(&entry);
      status->increment_num_local_overwrites();
      counters->num_local_overwrites++;
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict", OVERWRITE_LOCAL,
                                CONFLICT_RESOLUTION_SIZE);
    } else {
      if (entry.GetIsDir()) {
        Directory::Metahandles children;
        trans->directory()->GetChildHandlesById(trans, entry.GetId(),
                                                &children);
        // If a server deleted folder has local contents it should be a
        // hierarchy conflict.  Hierarchy conflicts should not be processed by
        // this function.
        DCHECK(children.empty());
      }

      // The entry is deleted on the server but still exists locally.
      // We undelete it by overwriting the server's tombstone with the local
      // data.
      conflict_util::OverwriteServerChanges(&entry);
      status->increment_num_server_overwrites();
      counters->num_server_overwrites++;
      DVLOG(1) << "Resolving simple conflict, undeleting server entry: "
               << entry;
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict", UNDELETE,
                                CONFLICT_RESOLUTION_SIZE);
    }
  }
}

void ConflictResolver::ResolveConflicts(
    syncable::WriteTransaction* trans,
    const Cryptographer* cryptographer,
    const std::set<syncable::Id>& simple_conflict_ids,
    StatusController* status,
    UpdateCounters* counters) {
  // Iterate over simple conflict items.
  set<Id>::const_iterator it;
  for (it = simple_conflict_ids.begin(); it != simple_conflict_ids.end();
       ++it) {
    // We don't resolve conflicts for control types here.
    Entry conflicting_node(trans, syncable::GET_BY_ID, *it);
    DCHECK(conflicting_node.good());
    if (IsControlType(
            GetModelTypeFromSpecifics(conflicting_node.GetSpecifics()))) {
      continue;
    }

    ProcessSimpleConflict(trans, *it, cryptographer, status, counters);
  }
  return;
}

}  // namespace syncer
