// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/commit_contribution_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/engine/sync_protocol_error.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace syncer {

namespace {

// When enabled, all the fields for SyncEntity are populated for commit-only
// data types (otherwise, only `specifics` and `id_string` were populated).
BASE_FEATURE(kSyncPopulateAllFieldsForCommitOnlyTypes,
             "SyncPopulateAllFieldsForCommitOnlyTypes",
             base::FEATURE_ENABLED_BY_DEFAULT);

CommitResponseData BuildCommitResponseData(
    const CommitRequestData& commit_request,
    const sync_pb::CommitResponse_EntryResponse& entry_response) {
  CommitResponseData response_data;
  response_data.id = entry_response.id_string();
  response_data.response_version = entry_response.version();
  response_data.client_tag_hash = commit_request.entity->client_tag_hash;
  response_data.sequence_number = commit_request.sequence_number;
  response_data.specifics_hash = commit_request.specifics_hash;
  response_data.unsynced_time = commit_request.unsynced_time;
  return response_data;
}

FailedCommitResponseData BuildFailedCommitResponseData(
    const CommitRequestData& commit_request,
    const sync_pb::CommitResponse_EntryResponse& entry_response) {
  FailedCommitResponseData response_data;
  response_data.client_tag_hash = commit_request.entity->client_tag_hash;
  response_data.response_type = entry_response.response_type();
  response_data.datatype_specific_error =
      entry_response.datatype_specific_error();
  return response_data;
}

}  // namespace

CommitContributionImpl::CommitContributionImpl(
    DataType type,
    const sync_pb::DataTypeContext& context,
    CommitRequestDataList commit_requests,
    base::OnceCallback<void(const CommitResponseDataList&,
                            const FailedCommitResponseDataList&)>
        on_commit_response_callback,
    base::OnceCallback<void(SyncCommitError)> on_full_commit_failure_callback,
    PassphraseType passphrase_type)
    : type_(type),
      on_commit_response_callback_(std::move(on_commit_response_callback)),
      on_full_commit_failure_callback_(
          std::move(on_full_commit_failure_callback)),
      passphrase_type_(passphrase_type),
      context_(context),
      commit_requests_(std::move(commit_requests)) {}

CommitContributionImpl::~CommitContributionImpl() = default;

void CommitContributionImpl::AddToCommitMessage(
    sync_pb::ClientToServerMessage* msg) {
  sync_pb::CommitMessage* commit_message = msg->mutable_commit();
  entries_start_index_ = commit_message->entries_size();

  commit_message->mutable_entries()->Reserve(commit_message->entries_size() +
                                             commit_requests_.size());

  for (const std::unique_ptr<CommitRequestData>& commit_request :
       commit_requests_) {
    sync_pb::SyncEntity* sync_entity = commit_message->add_entries();

    // Commit-only data types must never be encrypted or deleted.
    if (CommitOnlyTypes().Has(type_)) {
      CHECK(!commit_request->entity->specifics.has_encrypted());
      CHECK(!commit_request->entity->is_deleted());
    }

    if (CommitOnlyTypes().Has(type_) &&
        !base::FeatureList::IsEnabled(
            kSyncPopulateAllFieldsForCommitOnlyTypes)) {
      // Only send specifics to server for commit-only types.
      sync_entity->mutable_specifics()->CopyFrom(
          commit_request->entity->specifics);

      // Populate randomly-generated ID string similar to an uncommitted version
      // of normal data types.
      sync_entity->set_id_string(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
    } else {
      PopulateCommitProto(type_, *commit_request, sync_entity);
      AdjustCommitProto(sync_entity);
    }

    // Purposefully crash if we have client only data, as this could result in
    // sending password in plain text.
    CHECK(
        !sync_entity->specifics().password().has_client_only_encrypted_data());
    CHECK(!sync_entity->specifics()
               .outgoing_password_sharing_invitation()
               .has_client_only_unencrypted_data());
    CHECK(!sync_entity->specifics()
               .incoming_password_sharing_invitation()
               .has_client_only_unencrypted_data());

    // Purposefully crash since no metadata should be uploaded if a custom
    // passphrase is set.
    CHECK(!IsExplicitPassphrase(passphrase_type_) ||
          !sync_entity->specifics().password().has_unencrypted_metadata());

    // Record the size of the sync entity being committed.
    syncer::SyncRecordDataTypeEntitySizeHistogram(
        type_, commit_request->entity->is_deleted(),
        sync_entity->specifics().ByteSizeLong(), sync_entity->ByteSizeLong());

    if (commit_request->entity->is_deleted()) {
      RecordEntityChangeMetrics(type_, DataTypeEntityChange::kLocalDeletion);
    } else if (commit_request->base_version <= 0) {
      RecordEntityChangeMetrics(type_, DataTypeEntityChange::kLocalCreation);
    } else {
      RecordEntityChangeMetrics(type_, DataTypeEntityChange::kLocalUpdate);
    }
  }

  if (!context_.context().empty()) {
    commit_message->add_client_contexts()->CopyFrom(context_);
  }
}

SyncerError CommitContributionImpl::ProcessCommitResponse(
    const sync_pb::ClientToServerResponse& response,
    StatusController* status) {
  CommitResponseDataList success_response_list;
  FailedCommitResponseDataList error_response_list;
  bool has_invalid_messages = false;
  bool has_conflicting_commits = false;
  bool has_transient_error_commits = false;

  for (size_t i = 0; i < commit_requests_.size(); ++i) {
    // Fill |success_response_list| or |error_response_list|.
    const sync_pb::CommitResponse_EntryResponse& entry_response =
        response.commit().entryresponse(entries_start_index_ + i);
    if (entry_response.response_type() == sync_pb::CommitResponse::SUCCESS) {
      success_response_list.push_back(
          BuildCommitResponseData(*commit_requests_[i], entry_response));
    } else {
      error_response_list.push_back(
          BuildFailedCommitResponseData(*commit_requests_[i], entry_response));
    }

    // Update |status| and mark the presence of specific errors (e.g.
    // conflicting commits).
    switch (entry_response.response_type()) {
      case sync_pb::CommitResponse::SUCCESS:
        status->increment_num_successful_commits();
        if (type_ == BOOKMARKS) {
          status->increment_num_successful_bookmark_commits();
        }
        break;
      case sync_pb::CommitResponse::INVALID_MESSAGE:
        DLOG(ERROR) << "Server reports commit message is invalid.";
        has_invalid_messages = true;
        break;
      case sync_pb::CommitResponse::CONFLICT:
        DVLOG(1) << "Server reports conflict for commit message.";
        status->increment_num_server_conflicts();
        has_conflicting_commits = true;
        break;
      case sync_pb::CommitResponse::OVER_QUOTA:
      case sync_pb::CommitResponse::RETRY:
      case sync_pb::CommitResponse::TRANSIENT_ERROR:
        DLOG(WARNING) << "Entity commit blocked by transient error.";
        has_transient_error_commits = true;
        break;
    }
  }

  // Send whatever successful and failed responses we did get back to our
  // parent. It's the schedulers job to handle the failures, but parent may
  // react to them as well.
  std::move(on_commit_response_callback_)
      .Run(success_response_list, error_response_list);

  // Commit was successfully processed. We do not want to call both
  // |on_commit_response_callback_| and |on_full_commit_failure_callback_|.
  on_full_commit_failure_callback_.Reset();

  // Let the scheduler know about the failures.
  if (has_invalid_messages) {
    return SyncerError::ProtocolError(SyncProtocolErrorType::INVALID_MESSAGE);
  }
  if (has_transient_error_commits) {
    return SyncerError::ProtocolError(SyncProtocolErrorType::TRANSIENT_ERROR);
  }
  if (has_conflicting_commits) {
    return SyncerError::ProtocolError(SyncProtocolErrorType::CONFLICT);
  }
  return SyncerError::Success();
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
    DataType type,
    const CommitRequestData& commit_entity,
    sync_pb::SyncEntity* commit_proto) {
  const EntityData& entity_data = *commit_entity.entity;

  commit_proto->set_id_string(entity_data.id);

  if (type == NIGORI) {
    // Client tags are irrelevant for NIGORI since it uses the root node. For
    // historical reasons (although it's unclear if this continues to be
    // needed), the root node is considered a folder.
    commit_proto->set_folder(true);
  } else if (type != BOOKMARKS ||
             !entity_data.client_tag_hash.value().empty()) {
    // The client tag is mandatory for all datatypes except bookmarks, and
    // for bookmarks it depends on the version of the browser that was used
    // to originally create the bookmark.
    commit_proto->set_client_tag_hash(entity_data.client_tag_hash.value());
  }

  commit_proto->set_version(commit_entity.base_version);
  commit_proto->set_deleted(entity_data.is_deleted());
  commit_proto->set_name(entity_data.name);
  commit_proto->set_mtime(TimeToProtoTime(entity_data.modification_time));
  if (!entity_data.collaboration_id.empty()) {
    commit_proto->mutable_collaboration()->set_collaboration_id(
        entity_data.collaboration_id);
  }

  if (entity_data.is_deleted()) {
    if (entity_data.deletion_origin.has_value()) {
      *commit_proto->mutable_deletion_origin() = *entity_data.deletion_origin;
    }
  } else {
    // Handle bookmarks separately.
    if (type == BOOKMARKS) {
      // Populate SyncEntity.folder for backward-compatibility.
      commit_proto->set_folder(commit_entity.deprecated_bookmark_folder);
      CHECK(commit_entity.deprecated_bookmark_unique_position.IsValid());
      *commit_proto->mutable_unique_position() =
          commit_entity.deprecated_bookmark_unique_position.ToProto();

      // parent_id field is set only for legacy clients only, before M99.
      if (!entity_data.legacy_parent_id.empty()) {
        commit_proto->set_parent_id_string(entity_data.legacy_parent_id);
      }
    }
    commit_proto->set_ctime(TimeToProtoTime(entity_data.creation_time));
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
      commit_proto->set_id_string(
          base::Uuid::GenerateRandomV4().AsLowercaseString());
    }
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
