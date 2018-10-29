// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/directory_commit_contribution.h"

#include <algorithm>
#include <set>

#include "components/sync/engine/cycle/commit_counters.h"
#include "components/sync/engine_impl/commit_util.h"
#include "components/sync/engine_impl/get_commit_ids.h"
#include "components/sync/engine_impl/syncer_util.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/syncable_model_neutral_write_transaction.h"

namespace syncer {

using syncable::GET_BY_HANDLE;
using syncable::SYNCER;

DirectoryCommitContribution::~DirectoryCommitContribution() {
  DCHECK(!syncing_bits_set_);
}

// static.
std::unique_ptr<DirectoryCommitContribution> DirectoryCommitContribution::Build(
    syncable::Directory* dir,
    ModelType type,
    size_t max_entries,
    DataTypeDebugInfoEmitter* debug_info_emitter) {
  DCHECK(debug_info_emitter);

  std::vector<int64_t> metahandles;

  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir);
  GetCommitIdsForType(&trans, type, max_entries, &metahandles);

  if (metahandles.empty())
    return std::unique_ptr<DirectoryCommitContribution>();

  google::protobuf::RepeatedPtrField<sync_pb::SyncEntity> entities;
  for (auto it = metahandles.begin(); it != metahandles.end(); ++it) {
    sync_pb::SyncEntity* entity = entities.Add();
    syncable::ModelNeutralMutableEntry entry(&trans, GET_BY_HANDLE, *it);
    commit_util::BuildCommitItem(entry, entity);
    entry.PutSyncing(true);
  }

  sync_pb::DataTypeContext context;
  dir->GetDataTypeContext(&trans, type, &context);

  return std::unique_ptr<DirectoryCommitContribution>(
      new DirectoryCommitContribution(metahandles, entities, context, dir,
                                      debug_info_emitter));
}

void DirectoryCommitContribution::AddToCommitMessage(
    sync_pb::ClientToServerMessage* msg) {
  DCHECK(syncing_bits_set_);
  sync_pb::CommitMessage* commit_message = msg->mutable_commit();
  entries_start_index_ = commit_message->entries_size();
  std::copy(entities_.begin(), entities_.end(),
            RepeatedPtrFieldBackInserter(commit_message->mutable_entries()));
  if (!context_.context().empty())
    commit_message->add_client_contexts()->Swap(&context_);

  CommitCounters* counters = debug_info_emitter_->GetMutableCommitCounters();
  for (const sync_pb::SyncEntity& entity : entities_) {
    // Update the relevant counter based on the type of |entity|.
    if (entity.deleted()) {
      counters->num_deletion_commits_attempted++;
    } else if (entity.version() <= 0) {
      counters->num_creation_commits_attempted++;
    } else {
      counters->num_update_commits_attempted++;
    }
  }
}

SyncerError DirectoryCommitContribution::ProcessCommitResponse(
    const sync_pb::ClientToServerResponse& response,
    StatusController* status) {
  DCHECK(syncing_bits_set_);
  const sync_pb::CommitResponse& commit_response = response.commit();

  int transient_error_commits = 0;
  int conflicting_commits = 0;
  int error_commits = 0;
  int successes = 0;

  std::set<syncable::Id> deleted_folders;
  {
    syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
    for (size_t i = 0; i < metahandles_.size(); ++i) {
      sync_pb::CommitResponse::ResponseType response_type =
          commit_util::ProcessSingleCommitResponse(
              &trans, commit_response.entryresponse(entries_start_index_ + i),
              entities_.Get(i), metahandles_[i], &deleted_folders);
      switch (response_type) {
        case sync_pb::CommitResponse::INVALID_MESSAGE:
          ++error_commits;
          break;
        case sync_pb::CommitResponse::CONFLICT:
          ++conflicting_commits;
          status->increment_num_server_conflicts();
          break;
        case sync_pb::CommitResponse::SUCCESS:
          ++successes;
          {
            syncable::Entry e(&trans, GET_BY_HANDLE, metahandles_[i]);
            if (e.GetModelType() == BOOKMARKS)
              status->increment_num_successful_bookmark_commits();
          }
          status->increment_num_successful_commits();
          break;
        case sync_pb::CommitResponse::OVER_QUOTA:
        // We handle over quota like a retry, which is same as transient.
        case sync_pb::CommitResponse::RETRY:
        case sync_pb::CommitResponse::TRANSIENT_ERROR:
          ++transient_error_commits;
          break;
        default:
          LOG(FATAL) << "Bad return from ProcessSingleCommitResponse";
      }
    }
    MarkDeletedChildrenSynced(dir_, &trans, &deleted_folders);
  }

  CommitCounters* counters = debug_info_emitter_->GetMutableCommitCounters();
  counters->num_commits_success += successes;
  counters->num_commits_conflict += conflicting_commits;
  counters->num_commits_error += transient_error_commits;

  int commit_count = static_cast<int>(metahandles_.size());
  if (commit_count == successes) {
    return SYNCER_OK;
  } else if (error_commits > 0) {
    return SERVER_RETURN_UNKNOWN_ERROR;
  } else if (transient_error_commits > 0) {
    return SERVER_RETURN_TRANSIENT_ERROR;
  } else if (conflicting_commits > 0) {
    // This means that the server already has an item with this version, but
    // we haven't seen that update yet.
    //
    // A well-behaved client should respond to this by proceeding to the
    // download updates phase, fetching the conflicting items, then attempting
    // to resolve the conflict.  That's not what this client does.
    //
    // We don't currently have any code to support that exceptional control
    // flow.  Instead, we abort the current sync cycle and start a new one.  The
    // end result is the same.
    return SERVER_RETURN_CONFLICT;
  } else {
    LOG(FATAL) << "Inconsistent counts when processing commit response";
    return SYNCER_OK;
  }
}

void DirectoryCommitContribution::CleanUp() {
  DCHECK(syncing_bits_set_);
  UnsetSyncingBits();
  debug_info_emitter_->EmitCommitCountersUpdate();
  debug_info_emitter_->EmitStatusCountersUpdate();
}

size_t DirectoryCommitContribution::GetNumEntries() const {
  return metahandles_.size();
}

DirectoryCommitContribution::DirectoryCommitContribution(
    const std::vector<int64_t>& metahandles,
    const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>& entities,
    const sync_pb::DataTypeContext& context,
    syncable::Directory* dir,
    DataTypeDebugInfoEmitter* debug_info_emitter)
    : dir_(dir),
      metahandles_(metahandles),
      entities_(entities),
      context_(context),
      entries_start_index_(0xDEADBEEF),
      syncing_bits_set_(true),
      debug_info_emitter_(debug_info_emitter) {}

void DirectoryCommitContribution::UnsetSyncingBits() {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
  for (auto it = metahandles_.begin(); it != metahandles_.end(); ++it) {
    syncable::ModelNeutralMutableEntry entry(&trans, GET_BY_HANDLE, *it);
    // TODO(sync): this seems like it could be harmful if a sync cycle doesn't
    // complete but the Cleanup method is called anyways. It appears these are
    // unset on the assumption that the sync cycle must have finished properly,
    // although that's actually up to the commit response handling logic.
    entry.PutDirtySync(false);
    entry.PutSyncing(false);
  }
  syncing_bits_set_ = false;
}

}  // namespace syncer
