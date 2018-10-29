// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/commit_util.h"

#include <limits>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine_impl/syncer_proto_util.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/syncable_base_transaction.h"
#include "components/sync/syncable/syncable_base_write_transaction.h"
#include "components/sync/syncable/syncable_changes_version.h"
#include "components/sync/syncable/syncable_proto_util.h"
#include "components/sync/syncable/syncable_util.h"

using std::set;
using std::string;
using std::vector;

namespace syncer {

using syncable::Entry;
using syncable::Id;

namespace commit_util {

void AddExtensionsActivityToMessage(
    ExtensionsActivity* activity,
    ExtensionsActivity::Records* extensions_activity_buffer,
    sync_pb::CommitMessage* message) {
  // This isn't perfect, since the set of extensions activity may not correlate
  // exactly with the items being committed.  That's OK as long as we're looking
  // for a rough estimate of extensions activity, not an precise mapping of
  // which commits were triggered by which extension.
  //
  // We will push this list of extensions activity back into the
  // ExtensionsActivityMonitor if this commit fails.  That's why we must keep a
  // copy of these records in the cycle.
  activity->GetAndClearRecords(extensions_activity_buffer);

  const ExtensionsActivity::Records& records = *extensions_activity_buffer;
  for (auto it = records.begin(); it != records.end(); ++it) {
    sync_pb::ChromiumExtensionsActivity* activity_message =
        message->add_extensions_activity();
    activity_message->set_extension_id(it->second.extension_id);
    activity_message->set_bookmark_writes_since_last_commit(
        it->second.bookmark_write_count);
  }
}

void AddClientConfigParamsToMessage(ModelTypeSet enabled_types,
                                    bool cookie_jar_mismatch,
                                    sync_pb::CommitMessage* message) {
  sync_pb::ClientConfigParams* config_params = message->mutable_config_params();
  for (ModelType type : enabled_types) {
    if (ProxyTypes().Has(type))
      continue;
    int field_number = GetSpecificsFieldNumberFromModelType(type);
    config_params->mutable_enabled_type_ids()->Add(field_number);
  }
  config_params->set_tabs_datatype_enabled(enabled_types.Has(PROXY_TABS));
  config_params->set_cookie_jar_mismatch(cookie_jar_mismatch);
}

namespace {

void SetEntrySpecifics(const Entry& meta_entry,
                       sync_pb::SyncEntity* sync_entry) {
  // Add the new style extension and the folder bit.
  sync_entry->mutable_specifics()->CopyFrom(meta_entry.GetSpecifics());
  sync_entry->set_folder(meta_entry.GetIsDir());

  // Purposefully crash if we have client only data, as this could result in
  // sending password in plain text.
  CHECK(!sync_entry->specifics().password().has_client_only_encrypted_data());
  DCHECK_EQ(meta_entry.GetModelType(), GetModelType(*sync_entry));
}

}  // namespace

void BuildCommitItem(const syncable::Entry& meta_entry,
                     sync_pb::SyncEntity* sync_entry) {
  syncable::Id id = meta_entry.GetId();
  sync_entry->set_id_string(SyncableIdToProto(id));

  string name = meta_entry.GetNonUniqueName();
  DCHECK(!name.empty());  // Make sure this isn't an update.
  // Note: Truncation is also performed in WriteNode::SetTitle(..). But this
  // call is still necessary to handle any title changes that might originate
  // elsewhere, or already be persisted in the directory.
  base::TruncateUTF8ToByteSize(name, 255, &name);
  sync_entry->set_name(name);

  // Set the non_unique_name.  If we do, the server ignores
  // the |name| value (using |non_unique_name| instead), and will return
  // in the CommitResponse a unique name if one is generated.
  // We send both because it may aid in logging.
  sync_entry->set_non_unique_name(name);

  if (!meta_entry.GetUniqueClientTag().empty()) {
    sync_entry->set_client_defined_unique_tag(meta_entry.GetUniqueClientTag());
  }

  // Deleted items with server-unknown parent ids can be a problem so we set
  // the parent to 0. (TODO(sync): Still true in protocol?).
  Id new_parent_id;
  if (meta_entry.GetIsDel() && !meta_entry.GetParentId().ServerKnows()) {
    new_parent_id = syncable::BaseTransaction::root_id();
  } else {
    new_parent_id = meta_entry.GetParentId();
  }

  if (meta_entry.ShouldMaintainHierarchy()) {
    sync_entry->set_parent_id_string(SyncableIdToProto(new_parent_id));
  }

  // If our parent has changed, send up the old one so the server
  // can correctly deal with multiple parents.
  // TODO(nick): With the server keeping track of the primary sync parent,
  // it should not be necessary to provide the old_parent_id: the version
  // number should suffice.
  Id server_parent_id = meta_entry.GetServerParentId();
  if (new_parent_id != server_parent_id && !server_parent_id.IsNull() &&
      0 != meta_entry.GetBaseVersion() &&
      syncable::CHANGES_VERSION != meta_entry.GetBaseVersion()) {
    sync_entry->set_old_parent_id(SyncableIdToProto(server_parent_id));
  }

  int64_t version = meta_entry.GetBaseVersion();
  if (syncable::CHANGES_VERSION == version || 0 == version) {
    // Undeletions are only supported for items that have a client tag.
    DCHECK(!id.ServerKnows() || !meta_entry.GetUniqueClientTag().empty())
        << meta_entry;

    // Version 0 means to create or undelete an object.
    sync_entry->set_version(0);
  } else {
    DCHECK(id.ServerKnows()) << meta_entry;
    sync_entry->set_version(meta_entry.GetBaseVersion());
  }
  sync_entry->set_ctime(TimeToProtoTime(meta_entry.GetCtime()));
  sync_entry->set_mtime(TimeToProtoTime(meta_entry.GetMtime()));

  // Handle bookmarks separately.
  if (meta_entry.GetSpecifics().has_bookmark()) {
    if (meta_entry.GetIsDel()) {
      sync_entry->set_deleted(true);
    } else {
      // position_in_parent field is set only for legacy reasons.  See comments
      // in sync.proto for more information.
      sync_entry->set_position_in_parent(
          meta_entry.GetUniquePosition().ToInt64());
      sync_entry->mutable_unique_position()->CopyFrom(
          meta_entry.GetUniquePosition().ToProto());
    }
    // Always send specifics for bookmarks.
    SetEntrySpecifics(meta_entry, sync_entry);
    return;
  }

  // Deletion is final on the server, let's move things and then delete them.
  if (meta_entry.GetIsDel()) {
    sync_entry->set_deleted(true);

    sync_pb::EntitySpecifics type_only_specifics;
    AddDefaultFieldValue(meta_entry.GetModelType(),
                         sync_entry->mutable_specifics());
  } else {
    SetEntrySpecifics(meta_entry, sync_entry);
  }
}

// Helpers for ProcessSingleCommitResponse.
namespace {

void LogServerError(const sync_pb::CommitResponse_EntryResponse& res) {
  if (res.has_error_message())
    LOG(WARNING) << "  " << res.error_message();
  else
    LOG(WARNING) << "  No detailed error message returned from server";
}

const string& GetResultingPostCommitName(
    const sync_pb::SyncEntity& committed_entry,
    const sync_pb::CommitResponse_EntryResponse& entry_response) {
  const string& response_name =
      SyncerProtoUtil::NameFromCommitEntryResponse(entry_response);
  if (!response_name.empty())
    return response_name;
  return SyncerProtoUtil::NameFromSyncEntity(committed_entry);
}

bool UpdateVersionAfterCommit(
    const sync_pb::SyncEntity& committed_entry,
    const sync_pb::CommitResponse_EntryResponse& entry_response,
    const syncable::Id& pre_commit_id,
    syncable::ModelNeutralMutableEntry* local_entry) {
  int64_t old_version = local_entry->GetBaseVersion();
  int64_t new_version = entry_response.version();
  bool bad_commit_version = false;
  if (committed_entry.deleted() && !local_entry->GetUniqueClientTag().empty()) {
    // If the item was deleted, and it's undeletable (uses the client tag),
    // change the version back to zero.  We must set the version to zero so
    // that the server knows to re-create the item if it gets committed
    // later for undeletion.
    new_version = 0;
  } else if (!pre_commit_id.ServerKnows()) {
    bad_commit_version = 0 == new_version;
  } else {
    bad_commit_version = old_version > new_version;
  }
  if (bad_commit_version) {
    LOG(ERROR) << "Bad version in commit return for " << *local_entry
               << " new_id:" << SyncableIdFromProto(entry_response.id_string())
               << " new_version:" << entry_response.version();
    return false;
  }

  // Update the base version and server version.  The base version must change
  // here, even if syncing_was_set is false; that's because local changes were
  // on top of the successfully committed version.
  local_entry->PutBaseVersion(new_version);
  DVLOG(1) << "Commit is changing base version of " << local_entry->GetId()
           << " to: " << new_version;
  local_entry->PutServerVersion(new_version);
  return true;
}

bool ChangeIdAfterCommit(
    const sync_pb::CommitResponse_EntryResponse& entry_response,
    const syncable::Id& pre_commit_id,
    syncable::ModelNeutralMutableEntry* local_entry) {
  syncable::BaseWriteTransaction* trans = local_entry->base_write_transaction();
  const syncable::Id& entry_response_id =
      SyncableIdFromProto(entry_response.id_string());
  if (entry_response_id != pre_commit_id) {
    if (pre_commit_id.ServerKnows()) {
      // The server can sometimes generate a new ID on commit; for example,
      // when committing an undeletion.
      DVLOG(1) << " ID changed while committing an old entry. " << pre_commit_id
               << " became " << entry_response_id << ".";
    }
    syncable::ModelNeutralMutableEntry same_id(trans, syncable::GET_BY_ID,
                                               entry_response_id);
    // We should trap this before this function.
    if (same_id.good()) {
      LOG(ERROR) << "ID clash with id " << entry_response_id
                 << " during commit " << same_id;
      return false;
    }
    ChangeEntryIDAndUpdateChildren(trans, local_entry, entry_response_id);
    DVLOG(1) << "Changing ID to " << entry_response_id;
  }
  return true;
}

void UpdateServerFieldsAfterCommit(
    const sync_pb::SyncEntity& committed_entry,
    const sync_pb::CommitResponse_EntryResponse& entry_response,
    syncable::ModelNeutralMutableEntry* local_entry) {
  // We just committed an entry successfully, and now we want to make our view
  // of the server state consistent with the server state. We must be careful;
  // |entry_response| and |committed_entry| have some identically named
  // fields.  We only want to consider fields from |committed_entry| when there
  // is not an overriding field in the |entry_response|.  We do not want to
  // update the server data from the local data in the entry -- it's possible
  // that the local data changed during the commit, and even if not, the server
  // has the last word on the values of several properties.

  local_entry->PutServerIsDel(committed_entry.deleted());
  if (committed_entry.deleted()) {
    // Don't clobber any other fields of deleted objects.
    return;
  }

  local_entry->PutServerIsDir(committed_entry.folder());
  local_entry->PutServerSpecifics(committed_entry.specifics());
  local_entry->PutServerMtime(ProtoTimeToTime(committed_entry.mtime()));
  local_entry->PutServerCtime(ProtoTimeToTime(committed_entry.ctime()));
  if (committed_entry.has_unique_position()) {
    local_entry->PutServerUniquePosition(
        UniquePosition::FromProto(committed_entry.unique_position()));
  }

  // TODO(nick): The server doesn't set entry_response.server_parent_id in
  // practice; to update SERVER_PARENT_ID appropriately here we'd need to
  // get the post-commit ID of the parent indicated by
  // committed_entry.parent_id_string(). That should be inferrable from the
  // information we have, but it's a bit convoluted to pull it out directly.
  // Getting this right is important: SERVER_PARENT_ID gets fed back into
  // old_parent_id during the next commit.
  local_entry->PutServerParentId(local_entry->GetParentId());
  local_entry->PutServerNonUniqueName(
      GetResultingPostCommitName(committed_entry, entry_response));

  if (local_entry->GetIsUnappliedUpdate()) {
    // This shouldn't happen; an unapplied update shouldn't be committed, and
    // if it were, the commit should have failed.  But if it does happen: we've
    // just overwritten the update info, so clear the flag.
    local_entry->PutIsUnappliedUpdate(false);
  }
}

void ProcessSuccessfulCommitResponse(
    const sync_pb::SyncEntity& committed_entry,
    const sync_pb::CommitResponse_EntryResponse& entry_response,
    const syncable::Id& pre_commit_id,
    syncable::ModelNeutralMutableEntry* local_entry,
    bool dirty_sync_was_set,
    set<syncable::Id>* deleted_folders) {
  DCHECK(local_entry->GetIsUnsynced());

  // Update SERVER_VERSION and BASE_VERSION.
  if (!UpdateVersionAfterCommit(committed_entry, entry_response, pre_commit_id,
                                local_entry)) {
    LOG(ERROR) << "Bad version in commit return for " << *local_entry
               << " new_id:" << SyncableIdFromProto(entry_response.id_string())
               << " new_version:" << entry_response.version();
    return;
  }

  // If the server gave us a new ID, apply it.
  if (!ChangeIdAfterCommit(entry_response, pre_commit_id, local_entry)) {
    return;
  }

  // Update our stored copy of the server state.
  UpdateServerFieldsAfterCommit(committed_entry, entry_response, local_entry);

  // If the item doesn't need to be committed again (an item might need to be
  // committed again if it changed locally during the commit), we can remove
  // it from the unsynced list.
  if (!dirty_sync_was_set) {
    local_entry->PutIsUnsynced(false);
  }

  // Make a note of any deleted folders, whose children would have
  // been recursively deleted.
  // TODO(nick): Here, commit_message.deleted() would be more correct than
  // local_entry->GetIsDel().  For example, an item could be renamed, and then
  // deleted during the commit of the rename.  Unit test & fix.
  if (local_entry->GetIsDir() && local_entry->GetIsDel()) {
    deleted_folders->insert(local_entry->GetId());
  }
}

}  // namespace

sync_pb::CommitResponse::ResponseType ProcessSingleCommitResponse(
    syncable::BaseWriteTransaction* trans,
    const sync_pb::CommitResponse_EntryResponse& server_entry,
    const sync_pb::SyncEntity& commit_request_entry,
    int64_t metahandle,
    set<syncable::Id>* deleted_folders) {
  syncable::ModelNeutralMutableEntry local_entry(trans, syncable::GET_BY_HANDLE,
                                                 metahandle);
  DCHECK(local_entry.good());
  bool dirty_sync_was_set = local_entry.GetDirtySync();
  local_entry.PutDirtySync(false);
  local_entry.PutSyncing(false);

  sync_pb::CommitResponse::ResponseType response = server_entry.response_type();
  if (!sync_pb::CommitResponse::ResponseType_IsValid(response)) {
    LOG(ERROR) << "Commit response has unknown response type! Possibly out "
                  "of date client?";
    return sync_pb::CommitResponse::INVALID_MESSAGE;
  }
  if (sync_pb::CommitResponse::TRANSIENT_ERROR == response) {
    DVLOG(1) << "Transient Error Committing: " << local_entry;
    LogServerError(server_entry);
    return sync_pb::CommitResponse::TRANSIENT_ERROR;
  }
  if (sync_pb::CommitResponse::INVALID_MESSAGE == response) {
    LOG(ERROR) << "Error Committing: " << local_entry;
    LogServerError(server_entry);
    return response;
  }
  if (sync_pb::CommitResponse::CONFLICT == response) {
    DVLOG(1) << "Conflict Committing: " << local_entry;
    return response;
  }
  if (sync_pb::CommitResponse::RETRY == response) {
    DVLOG(1) << "Retry Committing: " << local_entry;
    return response;
  }
  if (sync_pb::CommitResponse::OVER_QUOTA == response) {
    LOG(WARNING) << "Hit deprecated OVER_QUOTA Committing: " << local_entry;
    return response;
  }
  if (!server_entry.has_id_string()) {
    LOG(ERROR) << "Commit response has no id";
    return sync_pb::CommitResponse::INVALID_MESSAGE;
  }

  // Implied by the IsValid call above, but here for clarity.
  DCHECK_EQ(sync_pb::CommitResponse::SUCCESS, response) << response;
  // Check to see if we've been given the ID of an existing entry. If so treat
  // it as an error response and retry later.
  const syncable::Id& server_entry_id =
      SyncableIdFromProto(server_entry.id_string());
  if (local_entry.GetId() != server_entry_id) {
    Entry e(trans, syncable::GET_BY_ID, server_entry_id);
    if (e.good()) {
      LOG(ERROR) << "Got duplicate id when committing id: "
                 << local_entry.GetId() << ". Treating as an error return";
      return sync_pb::CommitResponse::INVALID_MESSAGE;
    }
  }

  if (server_entry.version() == 0) {
    LOG(WARNING) << "Server returned a zero version on a commit response.";
  }

  ProcessSuccessfulCommitResponse(commit_request_entry, server_entry,
                                  local_entry.GetId(), &local_entry,
                                  dirty_sync_was_set, deleted_folders);
  return response;
}

}  // namespace commit_util

}  // namespace syncer
