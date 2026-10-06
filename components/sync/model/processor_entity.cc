// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/processor_entity.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/collaboration_metadata.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/version_info/version_info.h"

namespace syncer {

std::unique_ptr<ProcessorEntity> ProcessorEntity::CreateNew(
    const std::string& storage_key,
    const ClientTagHash& client_tag_hash,
    const std::string& server_id,
    base::Time creation_time) {
  return base::WrapUnique(new ProcessorEntity(
      storage_key, ProcessorEntityMetadata::CreateNew(
                       client_tag_hash, server_id, creation_time)));
}

std::unique_ptr<ProcessorEntity> ProcessorEntity::CreateFromMetadata(
    const std::string& storage_key,
    sync_pb::EntityMetadata metadata) {
  DCHECK(!storage_key.empty());
  std::unique_ptr<ProcessorEntityMetadata> entity_metadata =
      ProcessorEntityMetadata::FromProto(std::move(metadata));
  if (!entity_metadata) {
    return nullptr;
  }
  return base::WrapUnique(
      new ProcessorEntity(storage_key, std::move(*entity_metadata)));
}

ProcessorEntity::ProcessorEntity(const std::string& storage_key,
                                 ProcessorEntityMetadata metadata)
    : storage_key_(storage_key),
      metadata_(std::move(metadata)),
      commit_requested_sequence_number_(
          metadata_.proto().acked_sequence_number()) {}

ProcessorEntity::~ProcessorEntity() = default;

ClientTagHash ProcessorEntity::GetClientTagHash() const {
  return metadata_.GetClientTagHash();
}

void ProcessorEntity::SetStorageKey(const std::string& storage_key) {
  DCHECK(storage_key_.empty());
  DCHECK(!storage_key.empty());
  storage_key_ = storage_key;
}

void ProcessorEntity::ClearStorageKey() {
  storage_key_.clear();
}

void ProcessorEntity::SetCommitData(std::unique_ptr<EntityData> data) {
  DCHECK(data);
  // Update data's fields from metadata.
  data->client_tag_hash =
      ClientTagHash::FromHashed(metadata().client_tag_hash());
  if (!metadata().server_id().empty()) {
    data->id = metadata().server_id();
  }
  data->creation_time = ProtoTimeToTime(metadata().creation_time());
  data->modification_time = ProtoTimeToTime(metadata().modification_time());

  commit_data_ = std::move(data);
  // TODO(crbug.com/408182457): This DCHECK is sometimes violated for SESSIONS.
  DCHECK(HasCommitData());
}

bool ProcessorEntity::HasCommitData() const {
  return commit_data_ && !commit_data_->client_tag_hash.value().empty();
}

bool ProcessorEntity::MatchesData(const EntityData& data) const {
  return metadata_.MatchesData(data);
}

bool ProcessorEntity::MatchesOwnBaseData() const {
  return metadata_.MatchesOwnBaseData();
}

bool ProcessorEntity::MatchesBaseData(const EntityData& data) const {
  return metadata_.MatchesBaseData(data);
}

bool ProcessorEntity::IsUnsynced() const {
  return metadata_.IsUnsynced();
}

bool ProcessorEntity::IsUnsyncedLocalCreation() const {
  return metadata_.IsUnsyncedLocalCreation();
}

bool ProcessorEntity::RequiresCommitRequest() const {
  return metadata().sequence_number() > commit_requested_sequence_number_;
}

bool ProcessorEntity::RequiresCommitData() const {
  return RequiresCommitRequest() && !HasCommitData() &&
         !metadata().is_deleted();
}

bool ProcessorEntity::CanClearMetadata() const {
  return metadata().is_deleted() && !IsUnsynced();
}

bool ProcessorEntity::IsVersionAlreadyKnown(int64_t update_version) const {
  return metadata_.IsVersionAlreadyKnown(update_version);
}

void ProcessorEntity::UpdateCommitDataAndSequenceAfterRemoteUpdate() {
  // Either these already matched, acked was just bumped to squash a pending
  // commit and this should follow, or the pending commit needs to be requeued.
  commit_requested_sequence_number_ = metadata().acked_sequence_number();
  // If local change was made while server assigned a new id to the entity,
  // update id in cached commit data.
  if (HasCommitData() && commit_data_->id != metadata().server_id()) {
    DCHECK(commit_data_->id.empty());
    commit_data_->id = metadata().server_id();
  }
}

void ProcessorEntity::RecordIgnoredRemoteUpdate(
    const UpdateResponseData& update) {
  DCHECK(metadata().server_id().empty() ||
         metadata().server_id() == update.entity.id);
  metadata_.RecordIgnoredRemoteUpdate(update);
  UpdateCommitDataAndSequenceAfterRemoteUpdate();
}

void ProcessorEntity::RecordAcceptedRemoteUpdate(
    const UpdateResponseData& update,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  DCHECK(!IsUnsynced());
  metadata_.RecordAcceptedRemoteUpdate(update, std::move(trimmed_specifics),
                                       std::move(unique_position));
  UpdateCommitDataAndSequenceAfterRemoteUpdate();
}

void ProcessorEntity::RecordForcedRemoteUpdate(
    const UpdateResponseData& update,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  CHECK(IsUnsynced());
  commit_data_.reset();
  metadata_.RecordForcedRemoteUpdate(update, std::move(trimmed_specifics),
                                     std::move(unique_position));
  UpdateCommitDataAndSequenceAfterRemoteUpdate();
}

void ProcessorEntity::OverrideServerMetadata(const std::string& server_id,
                                             int64_t server_version) {
  metadata_.OverrideServerMetadata(server_id, server_version);
  if (commit_data_) {
    commit_data_->id = server_id;
  }
}

void ProcessorEntity::RecordLocalUpdate(
    std::unique_ptr<EntityData> data,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  DCHECK(!metadata().client_tag_hash().empty());
  metadata_.RecordLocalUpdate(*data, std::move(trimmed_specifics),
                              unique_position);
  SetCommitData(std::move(data));
}

bool ProcessorEntity::RecordLocalDeletion(const DeletionOrigin& origin) {
  commit_data_.reset();
  metadata_.RecordLocalDeletion(origin);
  // Return true if server might know about this entity.
  // TODO(crbug.com/41329567): This check will prevent sending tombstone in
  // situation when it should have been sent under following conditions:
  //  - Original entity was committed to server, but client crashed before
  //    receiving response.
  //  - Entity was deleted while client was offline.
  // Correct behavior is to send tombstone anyway, but the legacy Directory
  // implementation doesn't and it is unclear how server will react to such
  // tombstones. Change the behavior to always sending tombstone after
  // experimenting with server.
  return (metadata_.proto().server_version() != kUncommittedVersion) ||
         (commit_requested_sequence_number_ >
          metadata_.proto().acked_sequence_number());
}

void ProcessorEntity::InitializeCommitRequestData(CommitRequestData* request) {
  if (!metadata().is_deleted()) {
    DCHECK(HasCommitData());
    DCHECK_EQ(commit_data_->client_tag_hash.value(),
              metadata().client_tag_hash());
    DCHECK_EQ(commit_data_->id, metadata().server_id());
    request->entity = std::move(commit_data_);
  } else {
    // Make an EntityData with empty specifics to indicate deletion. This is
    // done lazily here to simplify loading a pending deletion on startup.
    auto data = std::make_unique<syncer::EntityData>();
    data->client_tag_hash =
        ClientTagHash::FromHashed(metadata().client_tag_hash());
    data->id = metadata().server_id();
    data->creation_time = ProtoTimeToTime(metadata().creation_time());
    data->modification_time = ProtoTimeToTime(metadata().modification_time());
    if (metadata().has_deletion_origin()) {
      data->deletion_origin = metadata().deletion_origin();
    }
    if (metadata().has_collaboration()) {
      data->collaboration_metadata =
          CollaborationMetadata::FromLocalProto(metadata().collaboration());
    }
    request->entity = std::move(data);
  }

  request->sequence_number = metadata().sequence_number();
  request->base_version = metadata().server_version();
  request->specifics_hash = metadata().specifics_hash();
  commit_requested_sequence_number_ = metadata().sequence_number();
}

void ProcessorEntity::ReceiveCommitResponse(const CommitResponseData& data,
                                            bool commit_only) {
  CHECK_EQ(metadata().client_tag_hash(), data.client_tag_hash.value());
  CHECK_GT(data.sequence_number, metadata().acked_sequence_number());
  // Version is not valid for commit only types, as it's stripped before being
  // sent to the server, so it cannot behave correctly.
  // Ignore the response if the server responds with an unexpected version.
  if (!commit_only && data.response_version <= metadata().server_version()) {
    return;
  }

  // The server can assign us a new ID in a commit response.
  metadata_.RecordCommitResponse(data);
  if (!IsUnsynced()) {
    // Clear pending commit data if there hasn't been another commit request
    // since the one that is currently getting acked.
    commit_data_.reset();
  } else {
    // If local change was made while server assigned a new id to the entity,
    // update id in cached commit data.
    if (HasCommitData() && commit_data_->id != metadata().server_id()) {
      commit_data_->id = metadata().server_id();
    }
  }
}

void ProcessorEntity::ClearTransientSyncState() {
  // If we have any unacknowledged commit requests outstanding, they've been
  // dropped and we should forget about them.
  commit_requested_sequence_number_ = metadata().acked_sequence_number();
}

void ProcessorEntity::IncrementSequenceNumber(base::Time modification_time) {
  metadata_.IncrementSequenceNumber();
}

size_t ProcessorEntity::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = metadata_.EstimateMemoryUsage();
  memory_usage += EstimateMemoryUsage(storage_key_);
  memory_usage += EstimateMemoryUsage(commit_data_);
  return memory_usage;
}


}  // namespace syncer
