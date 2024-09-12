// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/processor_entity.h"

#include <utility>

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/version_info/version_info.h"

namespace syncer {

namespace {

std::string HashSpecifics(const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  return base::Base64Encode(
      base::SHA1HashString(specifics.SerializeAsString()));
}

}  // namespace

std::unique_ptr<ProcessorEntity> ProcessorEntity::CreateNew(
    const std::string& storage_key,
    const ClientTagHash& client_tag_hash,
    const std::string& id,
    base::Time creation_time) {
  // Initialize metadata
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash(client_tag_hash.value());
  if (!id.empty()) {
    metadata.set_server_id(id);
  }
  metadata.set_sequence_number(0);
  metadata.set_acked_sequence_number(0);
  metadata.set_server_version(kUncommittedVersion);
  metadata.set_creation_time(TimeToProtoTime(creation_time));

  return base::WrapUnique(
      new ProcessorEntity(storage_key, std::move(metadata)));
}

std::unique_ptr<ProcessorEntity> ProcessorEntity::CreateFromMetadata(
    const std::string& storage_key,
    sync_pb::EntityMetadata metadata) {
  DCHECK(!storage_key.empty());
  return base::WrapUnique(
      new ProcessorEntity(storage_key, std::move(metadata)));
}

ProcessorEntity::ProcessorEntity(const std::string& storage_key,
                                 sync_pb::EntityMetadata metadata)
    : storage_key_(storage_key),
      commit_requested_sequence_number_(metadata.acked_sequence_number()) {
  DCHECK(metadata.has_client_tag_hash());
  DCHECK(metadata.has_creation_time());
  metadata_ = std::move(metadata);
}

ProcessorEntity::~ProcessorEntity() = default;

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
      ClientTagHash::FromHashed(metadata_.client_tag_hash());
  if (!metadata_.server_id().empty()) {
    data->id = metadata_.server_id();
  }
  data->creation_time = ProtoTimeToTime(metadata_.creation_time());
  data->modification_time = ProtoTimeToTime(metadata_.modification_time());

  commit_data_ = std::move(data);
  DCHECK(HasCommitData());
}

bool ProcessorEntity::HasCommitData() const {
  return commit_data_ && !commit_data_->client_tag_hash.value().empty();
}

bool ProcessorEntity::MatchesData(const EntityData& data) const {
  if (data.collaboration_id != metadata_.collaboration().collaboration_id()) {
    return false;
  }
  if (metadata_.is_deleted()) {
    return data.is_deleted();
  }
  if (data.is_deleted()) {
    return false;
  }
  // Do not check for unique position changes explicitly because they are
  // supposed to be in specifics.
  return MatchesSpecificsHash(data.specifics);
}

bool ProcessorEntity::MatchesOwnBaseData() const {
  DCHECK(IsUnsynced());
  if (metadata_.is_deleted()) {
    return false;
  }
  DCHECK(!metadata_.specifics_hash().empty());
  return metadata_.specifics_hash() == metadata_.base_specifics_hash();
}

bool ProcessorEntity::MatchesBaseData(const EntityData& data) const {
  DCHECK(IsUnsynced());
  if (data.is_deleted() || metadata_.base_specifics_hash().empty()) {
    return false;
  }
  return HashSpecifics(data.specifics) == metadata_.base_specifics_hash();
}

bool ProcessorEntity::IsUnsynced() const {
  return metadata_.sequence_number() > metadata_.acked_sequence_number();
}

// TODO(crbug.com/40725000): simplify the API and consider changing
// RequiresCommitRequest() with IsUnsynced().
bool ProcessorEntity::RequiresCommitRequest() const {
  return metadata_.sequence_number() > commit_requested_sequence_number_;
}

bool ProcessorEntity::RequiresCommitData() const {
  return RequiresCommitRequest() && !HasCommitData() && !metadata_.is_deleted();
}

bool ProcessorEntity::CanClearMetadata() const {
  return metadata_.is_deleted() && !IsUnsynced();
}

bool ProcessorEntity::IsVersionAlreadyKnown(int64_t update_version) const {
  return metadata_.server_version() >= update_version;
}

void ProcessorEntity::RecordIgnoredRemoteUpdate(
    const UpdateResponseData& update) {
  DCHECK(metadata_.server_id().empty() ||
         metadata_.server_id() == update.entity.id);
  metadata_.set_server_id(update.entity.id);
  metadata_.set_server_version(update.response_version);
  // Either these already matched, acked was just bumped to squash a pending
  // commit and this should follow, or the pending commit needs to be requeued.
  commit_requested_sequence_number_ = metadata_.acked_sequence_number();
  // If local change was made while server assigned a new id to the entity,
  // update id in cached commit data.
  if (HasCommitData() && commit_data_->id != metadata_.server_id()) {
    DCHECK(commit_data_->id.empty());
    commit_data_->id = metadata_.server_id();
  }
}

void ProcessorEntity::RecordAcceptedRemoteUpdate(
    const UpdateResponseData& update,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  DCHECK(!IsUnsynced());
  RecordIgnoredRemoteUpdate(update);
  metadata_.set_is_deleted(update.entity.is_deleted());
  metadata_.set_modification_time(
      TimeToProtoTime(update.entity.modification_time));
  if (!update.entity.collaboration_id.empty()) {
    metadata_.mutable_collaboration()->set_collaboration_id(
        update.entity.collaboration_id);
  }
  UpdateSpecificsHash(update.entity.specifics);
  *metadata_.mutable_possibly_trimmed_base_specifics() =
      std::move(trimmed_specifics);
  if (unique_position) {
    *metadata_.mutable_unique_position() = std::move(unique_position.value());
  } else {
    metadata_.clear_unique_position();
  }
}

void ProcessorEntity::RecordForcedRemoteUpdate(
    const UpdateResponseData& update,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  DCHECK(IsUnsynced());
  // There was a conflict and the server just won it. Explicitly ack all
  // pending commits so they are never enqueued again.
  metadata_.set_acked_sequence_number(metadata_.sequence_number());
  commit_data_.reset();
  RecordAcceptedRemoteUpdate(update, std::move(trimmed_specifics),
                             std::move(unique_position));
}

void ProcessorEntity::RecordLocalUpdate(
    std::unique_ptr<EntityData> data,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  DCHECK(!metadata_.client_tag_hash().empty());

  // Update metadata fields from updated data.
  base::Time modification_time = !data->modification_time.is_null()
                                     ? data->modification_time
                                     : base::Time::Now();

  // IncrementSequenceNumber should be called before UpdateSpecificHash since
  // it remembers specifics hash before the modifications.
  IncrementSequenceNumber(modification_time);
  UpdateSpecificsHash(data->specifics);
  *metadata_.mutable_possibly_trimmed_base_specifics() =
      std::move(trimmed_specifics);
  if (!data->creation_time.is_null()) {
    metadata_.set_creation_time(TimeToProtoTime(data->creation_time));
  }
  if (!data->collaboration_id.empty()) {
    metadata_.mutable_collaboration()->set_collaboration_id(
        data->collaboration_id);
  }
  metadata_.set_modification_time(TimeToProtoTime(modification_time));
  metadata_.set_is_deleted(false);
  if (unique_position) {
    *metadata_.mutable_unique_position() = std::move(unique_position.value());
  } else {
    metadata_.clear_unique_position();
  }

  // SetCommitData will update data's fields from metadata.
  SetCommitData(std::move(data));
}

bool ProcessorEntity::RecordLocalDeletion(const DeletionOrigin& origin) {
  IncrementSequenceNumber(base::Time::Now());
  metadata_.set_modification_time(TimeToProtoTime(base::Time::Now()));
  metadata_.set_is_deleted(true);
  metadata_.clear_specifics_hash();
  metadata_.clear_possibly_trimmed_base_specifics();
  metadata_.clear_unique_position();

  if (origin.is_specified()) {
    *metadata_.mutable_deletion_origin() =
        origin.ToProto(version_info::GetVersionNumber());
  }

  if (base::FeatureList::IsEnabled(
          syncer::kSyncEntityMetadataRecordDeletedByVersionOnLocalDeletion)) {
    metadata_.set_deleted_by_version(
        std::string(version_info::GetVersionNumber()));
  }

  // Clear any cached pending commit data.
  commit_data_.reset();
  // Return true if server might know about this entity.
  // TODO(crbug.com/41329567): This check will prevent sending tombstone in
  // situation when it should have been sent under following conditions:
  //  - Original centity was committed to server, but client crashed before
  //    receiving response.
  //  - Entity was deleted while client was offline.
  // Correct behavior is to send tombstone anyway, but the legacy Directory
  // implementation doesn't and it is unclear how server will react to such
  // tombstones. Change the behavior to always sending tombstone after
  // experimenting with server.
  return (metadata_.server_version() != kUncommittedVersion) ||
         (commit_requested_sequence_number_ >
          metadata_.acked_sequence_number());
}

void ProcessorEntity::InitializeCommitRequestData(CommitRequestData* request) {
  if (!metadata_.is_deleted()) {
    DCHECK(HasCommitData());
    DCHECK_EQ(commit_data_->client_tag_hash.value(),
              metadata_.client_tag_hash());
    DCHECK_EQ(commit_data_->id, metadata_.server_id());
    request->entity = std::move(commit_data_);
  } else {
    // Make an EntityData with empty specifics to indicate deletion. This is
    // done lazily here to simplify loading a pending deletion on startup.
    auto data = std::make_unique<syncer::EntityData>();
    data->client_tag_hash =
        ClientTagHash::FromHashed(metadata_.client_tag_hash());
    data->id = metadata_.server_id();
    data->creation_time = ProtoTimeToTime(metadata_.creation_time());
    data->modification_time = ProtoTimeToTime(metadata_.modification_time());
    if (metadata_.has_deletion_origin()) {
      data->deletion_origin = metadata_.deletion_origin();
    }
    if (metadata_.has_collaboration()) {
      data->collaboration_id = metadata_.collaboration().collaboration_id();
    }
    request->entity = std::move(data);
  }

  request->sequence_number = metadata_.sequence_number();
  request->base_version = metadata_.server_version();
  request->specifics_hash = metadata_.specifics_hash();
  request->unsynced_time = unsynced_time_;
  commit_requested_sequence_number_ = metadata_.sequence_number();
}

void ProcessorEntity::ReceiveCommitResponse(const CommitResponseData& data,
                                            bool commit_only) {
  DCHECK_EQ(metadata_.client_tag_hash(), data.client_tag_hash.value());
  DCHECK_GT(data.sequence_number, metadata_.acked_sequence_number());
  // Version is not valid for commit only types, as it's stripped before being
  // sent to the server, so it cannot behave correctly.
  // Ignore the response if the server responds with an unexpected version.
  if (!commit_only && data.response_version <= metadata_.server_version()) {
    return;
  }

  // The server can assign us a new ID in a commit response.
  metadata_.set_server_id(data.id);
  metadata_.set_acked_sequence_number(data.sequence_number);
  metadata_.set_server_version(data.response_version);
  if (!IsUnsynced()) {
    // Clear pending commit data if there hasn't been another commit request
    // since the one that is currently getting acked.
    commit_data_.reset();
    metadata_.clear_base_specifics_hash();
  } else {
    metadata_.set_base_specifics_hash(data.specifics_hash);
    // If local change was made while server assigned a new id to the entity,
    // update id in cached commit data.
    if (HasCommitData() && commit_data_->id != metadata_.server_id()) {
      DCHECK(commit_data_->id.empty());
      commit_data_->id = metadata_.server_id();
    }
  }
}

void ProcessorEntity::ClearTransientSyncState() {
  // If we have any unacknowledged commit requests outstanding, they've been
  // dropped and we should forget about them.
  commit_requested_sequence_number_ = metadata_.acked_sequence_number();
}

void ProcessorEntity::IncrementSequenceNumber(base::Time modification_time) {
  DCHECK(metadata_.has_sequence_number());
  if (!IsUnsynced()) {
    // Update the base specifics hash if this entity wasn't already out of sync.
    metadata_.set_base_specifics_hash(metadata_.specifics_hash());
    unsynced_time_ = modification_time;
  }
  metadata_.set_sequence_number(metadata_.sequence_number() + 1);
  DCHECK(IsUnsynced());
}

size_t ProcessorEntity::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(storage_key_);
  memory_usage += EstimateMemoryUsage(metadata_);
  memory_usage += EstimateMemoryUsage(commit_data_);
  return memory_usage;
}

bool ProcessorEntity::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  DCHECK(!metadata_.is_deleted());
  DCHECK_GT(specifics.ByteSize(), 0);
  return HashSpecifics(specifics) == metadata_.specifics_hash();
}

void ProcessorEntity::UpdateSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) {
  if (specifics.ByteSize() > 0) {
    *metadata_.mutable_specifics_hash() = HashSpecifics(specifics);
  } else {
    metadata_.clear_specifics_hash();
  }
}

}  // namespace syncer
