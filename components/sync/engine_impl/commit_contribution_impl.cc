// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/commit_contribution_impl.h"

#include <algorithm>
#include <utility>

#include "base/guid.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine_impl/cycle/entity_change_metric_recording.h"
#include "components/sync/engine_impl/model_type_worker.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

CommitContributionImpl::CommitContributionImpl(
    ModelType type,
    const sync_pb::DataTypeContext& context,
    CommitRequestDataList commit_requests,
    base::OnceCallback<void(const CommitResponseDataList&,
                            const FailedCommitResponseDataList&)>
        on_commit_response_callback,
    base::OnceCallback<void(SyncCommitError)> on_full_commit_failure_callback,
    Cryptographer* cryptographer,
    PassphraseType passphrase_type,
    bool only_commit_specifics)
    : type_(type),
      on_commit_response_callback_(std::move(on_commit_response_callback)),
      on_full_commit_failure_callback_(
          std::move(on_full_commit_failure_callback)),
      cryptographer_(cryptographer),
      passphrase_type_(passphrase_type),
      context_(context),
      commit_requests_(std::move(commit_requests)),
      only_commit_specifics_(only_commit_specifics) {}

CommitContributionImpl::~CommitContributionImpl() = default;

void CommitContributionImpl::AddToCommitMessage(
    sync_pb::ClientToServerMessage* msg) {
  sync_pb::CommitMessage* commit_message = msg->mutable_commit();
  entries_start_index_ = commit_message->entries_size();

  commit_message->mutable_entries()->Reserve(commit_message->entries_size() +
                                             commit_requests_.size());

  for (const auto& commit_request : commit_requests_) {
    sync_pb::SyncEntity* sync_entity = commit_message->add_entries();
    if (only_commit_specifics_) {
      DCHECK(!commit_request->entity->is_deleted());
      DCHECK(!cryptographer_);
      // Only send specifics to server for commit-only types.
      sync_entity->mutable_specifics()->CopyFrom(
          commit_request->entity->specifics);
    } else {
      PopulateCommitProto(type_, *commit_request, sync_entity);
      AdjustCommitProto(sync_entity);
    }

    // Purposefully crash if we have client only data, as this could result in
    // sending password in plain text.
    CHECK(
        !sync_entity->specifics().password().has_client_only_encrypted_data());

    if (commit_request->entity->is_deleted()) {
      RecordEntityChangeMetrics(type_, ModelTypeEntityChange::kLocalDeletion);
    } else if (commit_request->base_version <= 0) {
      RecordEntityChangeMetrics(type_, ModelTypeEntityChange::kLocalCreation);
    } else {
      RecordEntityChangeMetrics(type_, ModelTypeEntityChange::kLocalUpdate);
    }
  }

  if (!context_.context().empty())
    commit_message->add_client_contexts()->CopyFrom(context_);
}

SyncerError CommitContributionImpl::ProcessCommitResponse(
    const sync_pb::ClientToServerResponse& response,
    StatusController* status) {
  const sync_pb::CommitResponse& commit_response = response.commit();

  bool unknown_error = false;
  int transient_error_commits = 0;
  int conflicting_commits = 0;
  int successes = 0;

  CommitResponseDataList committed_response_list;
  FailedCommitResponseDataList error_response_list;

  for (size_t i = 0; i < commit_requests_.size(); ++i) {
    const sync_pb::CommitResponse_EntryResponse& entry_response =
        commit_response.entryresponse(entries_start_index_ + i);
    const CommitRequestData& commit_request = *commit_requests_[i];

    if (entry_response.response_type() == sync_pb::CommitResponse::SUCCESS) {
      ++successes;

      CommitResponseData response_data;
      response_data.id = entry_response.id_string();

      if (response_data.id != commit_request.entity->id) {
        // Server has changed the sync id in the request. Write back the
        // original sync id. This is useful for data types without a notion of
        // a client tag such as bookmarks.
        response_data.id_in_request = commit_request.entity->id;
      }
      response_data.response_version = entry_response.version();
      response_data.client_tag_hash = commit_request.entity->client_tag_hash;
      response_data.sequence_number = commit_request.sequence_number;
      response_data.specifics_hash = commit_request.specifics_hash;
      response_data.unsynced_time = commit_request.unsynced_time;
      committed_response_list.push_back(response_data);

      status->increment_num_successful_commits();
      if (type_ == BOOKMARKS) {
        status->increment_num_successful_bookmark_commits();
      }
    } else {
      FailedCommitResponseData response_data;
      response_data.client_tag_hash = commit_request.entity->client_tag_hash;

      response_data.response_type = entry_response.response_type();
      response_data.datatype_specific_error =
          entry_response.datatype_specific_error();
      error_response_list.push_back(response_data);

      switch (entry_response.response_type()) {
        case sync_pb::CommitResponse::INVALID_MESSAGE:
          DLOG(ERROR) << "Server reports commit message is invalid.";
          unknown_error = true;
          break;
        case sync_pb::CommitResponse::CONFLICT:
          DVLOG(1) << "Server reports conflict for commit message.";
          ++conflicting_commits;
          status->increment_num_server_conflicts();
          break;
        case sync_pb::CommitResponse::OVER_QUOTA:
        case sync_pb::CommitResponse::RETRY:
        case sync_pb::CommitResponse::TRANSIENT_ERROR:
          DLOG(WARNING) << "Entity commit blocked by transient error.";
          ++transient_error_commits;
          break;
        // TODO(vitaliii): avoid the default clause and list all values
        // explicitly (this will fail at compile time if enum is extended).
        default:
          DLOG(ERROR) << "Bad commit response.";
          unknown_error = true;
      }
    }
  }

  // Send whatever successful and failed responses we did get back to our
  // parent. It's the schedulers job to handle the failures, but parent may
  // react to them as well.
  std::move(on_commit_response_callback_)
      .Run(committed_response_list, error_response_list);

  // Commit was successfully processed. We do not want to call both
  // |on_commit_response_callback_| and |on_full_commit_failure_callback_|.
  on_full_commit_failure_callback_.Reset();

  // Let the scheduler know about the failures.
  if (unknown_error) {
    return SyncerError(SyncerError::SERVER_RETURN_UNKNOWN_ERROR);
  } else if (transient_error_commits > 0) {
    return SyncerError(SyncerError::SERVER_RETURN_TRANSIENT_ERROR);
  } else if (conflicting_commits > 0) {
    return SyncerError(SyncerError::SERVER_RETURN_CONFLICT);
  } else {
    return SyncerError(SyncerError::SYNCER_OK);
  }
}

void CommitContributionImpl::ProcessCommitFailure(
    SyncCommitError commit_error) {
  std::move(on_full_commit_failure_callback_).Run(commit_error);
  on_commit_response_callback_.Reset();
}

size_t CommitContributionImpl::GetNumEntries() const {
  return commit_requests_.size();
}

// static
void CommitContributionImpl::PopulateCommitProto(
    ModelType type,
    const CommitRequestData& commit_entity,
    sync_pb::SyncEntity* commit_proto) {
  const EntityData& entity_data = *commit_entity.entity;
  commit_proto->set_id_string(entity_data.id);
  // Populate client_defined_unique_tag only for non-bookmark and non-Nigori
  // data types.
  if (type != BOOKMARKS && type != NIGORI) {
    commit_proto->set_client_defined_unique_tag(
        entity_data.client_tag_hash.value());
  }
  commit_proto->set_version(commit_entity.base_version);
  commit_proto->set_deleted(entity_data.is_deleted());
  commit_proto->set_folder(entity_data.is_folder);
  commit_proto->set_name(entity_data.name);

  if (!entity_data.is_deleted()) {
    // Handle bookmarks separately.
    if (type == BOOKMARKS) {
      // position_in_parent field is set only for legacy reasons.  See comments
      // in sync.proto for more information.
      const UniquePosition unique_position =
          UniquePosition::FromProto(entity_data.unique_position);
      if (unique_position.IsValid()) {
        commit_proto->set_position_in_parent(unique_position.ToInt64());
      }
      commit_proto->mutable_unique_position()->CopyFrom(
          entity_data.unique_position);
      // TODO(mamir): check if parent_id_string needs to be populated for
      // non-deletions.
      if (!entity_data.parent_id.empty()) {
        commit_proto->set_parent_id_string(entity_data.parent_id);
      }
    }
    commit_proto->set_ctime(TimeToProtoTime(entity_data.creation_time));
    commit_proto->set_mtime(TimeToProtoTime(entity_data.modification_time));
    commit_proto->mutable_specifics()->CopyFrom(entity_data.specifics);
  }
}

void CommitContributionImpl::AdjustCommitProto(
    sync_pb::SyncEntity* commit_proto) {
  if (commit_proto->version() == kUncommittedVersion) {
    commit_proto->set_version(0);
    // Initial commits need our help to generate a client ID if they don't have
    // any. Bookmarks create their own IDs on the frontend side to be able to
    // match them after commits. For other data types we generate one here. And
    // since bookmarks don't have client tags, their server id should be stable
    // across restarts in case of recommitting an item, it doesn't result in
    // creating a duplicate.
    if (commit_proto->id_string().empty()) {
      commit_proto->set_id_string(base::GenerateGUID());
    }
  }

  // Encrypt the specifics and hide the title if necessary.
  if (commit_proto->specifics().has_password()) {
    DCHECK(cryptographer_);
    const sync_pb::PasswordSpecifics& password_specifics =
        commit_proto->specifics().password();
    const sync_pb::PasswordSpecificsData& password_data =
        password_specifics.client_only_encrypted_data();
    sync_pb::EntitySpecifics encrypted_password;
    if (!IsExplicitPassphrase(passphrase_type_) &&
        password_specifics.unencrypted_metadata().url() !=
            password_data.signon_realm()) {
      encrypted_password.mutable_password()
          ->mutable_unencrypted_metadata()
          ->set_url(password_data.signon_realm());
      encrypted_password.mutable_password()
          ->mutable_unencrypted_metadata()
          ->set_blacklisted(password_data.blacklisted());
    }

    bool result = cryptographer_->Encrypt(
        password_data,
        encrypted_password.mutable_password()->mutable_encrypted());
    DCHECK(result);
    *commit_proto->mutable_specifics() = std::move(encrypted_password);
    commit_proto->set_name("encrypted");
  } else if (cryptographer_) {
    if (commit_proto->has_specifics()) {
      sync_pb::EntitySpecifics encrypted_specifics;
      bool result = cryptographer_->Encrypt(
          commit_proto->specifics(), encrypted_specifics.mutable_encrypted());
      DCHECK(result);
      commit_proto->mutable_specifics()->CopyFrom(encrypted_specifics);
    }
    commit_proto->set_name("encrypted");
  }

  // See crbug.com/915133: Certain versions of Chrome (e.g. M71) handle corrupt
  // SESSIONS data poorly. Let's guard against future versions from committing
  // problematic data that could cause crashes on other syncing devices.
  if (commit_proto->specifics().session().has_tab()) {
    CHECK_GE(commit_proto->specifics().session().tab_node_id(), 0);
  }

  // Always include enough specifics to identify the type. Do this even in
  // deletion requests, where the specifics are otherwise invalid.
  AddDefaultFieldValue(type_, commit_proto->mutable_specifics());
}

}  // namespace syncer
