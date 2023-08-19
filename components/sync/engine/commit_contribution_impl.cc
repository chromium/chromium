// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/commit_contribution_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/features.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/engine/model_type_worker.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace syncer {

namespace {

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

  for (const std::unique_ptr<CommitRequestData>& commit_request :
       commit_requests_) {
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
    CHECK(!sync_entity->specifics()
               .outgoing_password_sharing_invitation()
               .has_client_only_unencrypted_data());

    // Purposefully crash since no metadata should be uploaded if a custom
    // passphrase is set.
    CHECK(!IsExplicitPassphrase(passphrase_type_) ||
          !sync_entity->specifics().password().has_unencrypted_metadata());

    // Record the size of the sync entity being committed.
    syncer::SyncRecordModelTypeEntitySizeHistogram(
        type_, sync_entity->specifics().ByteSizeLong());

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
  CommitResponseDataList success_response_list;
  FailedCommitResponseDataList error_response_list;
  bool has_unknown_error = false;
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
        has_unknown_error = true;
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
  if (has_unknown_error) {
    return SyncerError(SyncerError::SERVER_RETURN_UNKNOWN_ERROR);
  }
  if (has_transient_error_commits) {
    return SyncerError(SyncerError::SERVER_RETURN_TRANSIENT_ERROR);
  }
  if (has_conflicting_commits) {
    return SyncerError(SyncerError::SERVER_RETURN_CONFLICT);
  }
  return SyncerError(SyncerError::SYNCER_OK);
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
  DCHECK(!entity_data.specifics.has_encrypted());

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

  if (!entity_data.is_deleted()) {
    // Handle bookmarks separately.
    if (type == BOOKMARKS) {
      // Populate SyncEntity.folder for backward-compatibility.
      switch (entity_data.specifics.bookmark().type()) {
        case sync_pb::BookmarkSpecifics::UNSPECIFIED:
          NOTREACHED();
          break;
        case sync_pb::BookmarkSpecifics::URL:
          commit_proto->set_folder(false);
          break;
        case sync_pb::BookmarkSpecifics::FOLDER:
          commit_proto->set_folder(true);
          break;
      }
      const UniquePosition unique_position = UniquePosition::FromProto(
          entity_data.specifics.bookmark().unique_position());
      DCHECK(unique_position.IsValid());
      *commit_proto->mutable_unique_position() = unique_position.ToProto();
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

  if (commit_proto->specifics().has_password()) {
    EncryptPasswordSpecificsData(commit_proto);
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

void CommitContributionImpl::EncryptPasswordSpecificsData(
    sync_pb::SyncEntity* commit_proto) {
  DCHECK(cryptographer_);
  const sync_pb::PasswordSpecifics& password_specifics =
      commit_proto->specifics().password();
  const sync_pb::PasswordSpecificsData& password_data =
      password_specifics.client_only_encrypted_data();
  sync_pb::EntitySpecifics encrypted_password;

  // Keep the unencrypted metadata for non-custom passphrase users.
  if (!IsExplicitPassphrase(passphrase_type_)) {
    *encrypted_password.mutable_password()->mutable_unencrypted_metadata() =
        commit_proto->specifics().password().unencrypted_metadata();
  }

  bool result = cryptographer_->Encrypt(
      password_data,
      encrypted_password.mutable_password()->mutable_encrypted());
  DCHECK(result);
  if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
    // `encrypted_notes_backup` field needs to be populated regardless of
    // whether or not there are any notes.
    result = cryptographer_->Encrypt(password_data.notes(),
                                     encrypted_password.mutable_password()
                                         ->mutable_encrypted_notes_backup());
    DCHECK(result);
    // When encrypting both blobs succeeds, both encrypted blobs must use the
    // key name.
    DCHECK_EQ(
        encrypted_password.password().encrypted().key_name(),
        encrypted_password.password().encrypted_notes_backup().key_name());
  }
  *commit_proto->mutable_specifics() = std::move(encrypted_password);
  commit_proto->set_name("encrypted");
}

}  // namespace syncer
