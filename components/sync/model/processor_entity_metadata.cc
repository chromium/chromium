// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/processor_entity_metadata.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/collaboration_metadata.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/version_info/version_info.h"

namespace syncer {

namespace {

std::string HashSpecifics(const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSizeLong(), 0u);
  return base::Base64Encode(
      base::SHA1HashString(specifics.SerializeAsString()));
}

}  // namespace

// static
bool ProcessorEntityMetadata::IsValid(const sync_pb::EntityMetadata& metadata) {
  return !metadata.client_tag_hash().empty() && metadata.has_creation_time() &&
         metadata.sequence_number() >= metadata.acked_sequence_number();
}

// static
ProcessorEntityMetadata ProcessorEntityMetadata::CreateNew(
    const ClientTagHash& client_tag_hash,
    std::string server_id,
    base::Time creation_time) {
  sync_pb::EntityMetadata metadata;
  metadata.set_client_tag_hash(client_tag_hash.value());
  if (!server_id.empty()) {
    metadata.set_server_id(std::move(server_id));
  }
  metadata.set_sequence_number(0);
  metadata.set_acked_sequence_number(0);
  metadata.set_server_version(kUncommittedVersion);
  metadata.set_creation_time(TimeToProtoTime(creation_time));
  return ProcessorEntityMetadata(std::move(metadata));
}

// static
std::unique_ptr<ProcessorEntityMetadata> ProcessorEntityMetadata::FromProto(
    sync_pb::EntityMetadata metadata) {
  if (!IsValid(metadata)) {
    return nullptr;
  }
  return base::WrapUnique(new ProcessorEntityMetadata(std::move(metadata)));
}

ProcessorEntityMetadata::ProcessorEntityMetadata(
    sync_pb::EntityMetadata metadata)
    : metadata_(std::move(metadata)) {
  CHECK(IsValid(metadata_));
}

ProcessorEntityMetadata::~ProcessorEntityMetadata() = default;

bool ProcessorEntityMetadata::IsDeleted() const {
  return metadata_.is_deleted();
}

bool ProcessorEntityMetadata::IsUnsynced() const {
  return metadata_.sequence_number() > metadata_.acked_sequence_number();
}

bool ProcessorEntityMetadata::IsUnsyncedLocalCreation() const {
  return metadata_.server_version() == kUncommittedVersion && IsUnsynced();
}

bool ProcessorEntityMetadata::IsVersionAlreadyKnown(
    int64_t update_version) const {
  return metadata_.server_version() >= update_version;
}

void ProcessorEntityMetadata::OverrideServerMetadata(
    const std::string& server_id,
    int64_t server_version) {
  metadata_.set_server_id(server_id);
  metadata_.set_server_version(server_version);
}

size_t ProcessorEntityMetadata::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(metadata_);
}

ClientTagHash ProcessorEntityMetadata::GetClientTagHash() const {
  return ClientTagHash::FromHashed(metadata_.client_tag_hash());
}

void ProcessorEntityMetadata::IncrementSequenceNumber() {
  CHECK(metadata_.has_sequence_number(), base::NotFatalUntil::M141);
  if (!IsUnsynced()) {
    // Update the base specifics hash if this entity wasn't already out of sync.
    metadata_.set_base_specifics_hash(metadata_.specifics_hash());
  }
  metadata_.set_sequence_number(metadata_.sequence_number() + 1);
  CHECK(IsUnsynced(), base::NotFatalUntil::M141);
}

bool ProcessorEntityMetadata::MatchesData(const EntityData& data) const {
  if (metadata_.is_deleted() || data.is_deleted()) {
    // If either of them is deleted, the data matches only if both are deleted.
    return metadata_.is_deleted() == data.is_deleted();
  }
  // Do not check for unique position changes explicitly because they are
  // supposed to be in specifics.
  return MatchesSpecificsHash(data.specifics);
}

bool ProcessorEntityMetadata::MatchesBaseData(const EntityData& data) const {
  DCHECK(IsUnsynced());
  if (data.is_deleted() || metadata_.base_specifics_hash().empty()) {
    return false;
  }
  return HashSpecifics(data.specifics) == metadata_.base_specifics_hash();
}

bool ProcessorEntityMetadata::MatchesOwnBaseData() const {
  DCHECK(IsUnsynced());
  if (metadata_.is_deleted()) {
    return false;
  }
  DCHECK(!metadata_.specifics_hash().empty());
  return metadata_.specifics_hash() == metadata_.base_specifics_hash();
}

bool ProcessorEntityMetadata::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  DCHECK(!metadata_.is_deleted());
  DCHECK_GT(specifics.ByteSizeLong(), 0u);
  return HashSpecifics(specifics) == metadata_.specifics_hash();
}

bool ProcessorEntityMetadata::MatchesFaviconHash(
    const std::string& favicon_png_bytes) const {
  DCHECK(!metadata_.is_deleted());
  return metadata_.bookmark_favicon_hash() ==
         base::PersistentHash(favicon_png_bytes);
}

void ProcessorEntityMetadata::UpdateSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) {
  if (specifics.ByteSizeLong() > 0) {
    metadata_.set_specifics_hash(HashSpecifics(specifics));
    if (specifics.has_bookmark()) {
      metadata_.set_bookmark_favicon_hash(
          base::PersistentHash(specifics.bookmark().favicon()));
    }
  } else {
    metadata_.clear_specifics_hash();
    metadata_.clear_bookmark_favicon_hash();
  }
}

void ProcessorEntityMetadata::RecordIgnoredRemoteUpdate(
    const UpdateResponseData& update) {
  metadata_.set_server_id(update.entity.id);
  metadata_.set_server_version(update.response_version);
}

void ProcessorEntityMetadata::RecordAcceptedRemoteUpdate(
    const UpdateResponseData& update,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  RecordIgnoredRemoteUpdate(update);
  metadata_.set_is_deleted(update.entity.is_deleted());
  metadata_.set_modification_time(
      TimeToProtoTime(update.entity.modification_time));
  if (update.entity.collaboration_metadata.has_value()) {
    metadata_.mutable_collaboration()->set_collaboration_id(
        update.entity.collaboration_metadata->collaboration_id().value());
    if (!update.entity.collaboration_metadata->created_by().empty()) {
      metadata_.mutable_collaboration()
          ->mutable_creation_attribution()
          ->set_obfuscated_gaia_id(
              update.entity.collaboration_metadata->created_by().ToString());
    }
    if (!update.entity.collaboration_metadata->last_updated_by().empty()) {
      metadata_.mutable_collaboration()
          ->mutable_last_update_attribution()
          ->set_obfuscated_gaia_id(
              update.entity.collaboration_metadata->last_updated_by()
                  .ToString());
    }
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

void ProcessorEntityMetadata::RecordForcedRemoteUpdate(
    const UpdateResponseData& update,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  metadata_.set_acked_sequence_number(metadata_.sequence_number());
  RecordAcceptedRemoteUpdate(update, std::move(trimmed_specifics),
                             std::move(unique_position));
}

void ProcessorEntityMetadata::RecordCommitResponse(
    const CommitResponseData& data) {
  metadata_.set_server_id(data.id);
  metadata_.set_acked_sequence_number(data.sequence_number);
  metadata_.set_server_version(data.response_version);
  if (!IsUnsynced()) {
    metadata_.clear_base_specifics_hash();
  } else {
    metadata_.set_base_specifics_hash(data.specifics_hash);
  }
}

void ProcessorEntityMetadata::UpdateMetadataForLocalUpdate(
    const sync_pb::EntitySpecifics& specifics,
    base::Time modification_time,
    std::optional<sync_pb::UniquePosition> unique_position) {
  IncrementSequenceNumber();
  UpdateSpecificsHash(specifics);
  metadata_.set_modification_time(TimeToProtoTime(modification_time));
  metadata_.set_is_deleted(false);
  if (unique_position.has_value()) {
    *metadata_.mutable_unique_position() = std::move(unique_position).value();
  } else {
    metadata_.clear_unique_position();
  }
}

void ProcessorEntityMetadata::RecordLocalUpdate(
    const EntityData& data,
    sync_pb::EntitySpecifics trimmed_specifics,
    std::optional<sync_pb::UniquePosition> unique_position) {
  base::Time modification_time = !data.modification_time.is_null()
                                     ? data.modification_time
                                     : base::Time::Now();

  UpdateMetadataForLocalUpdate(data.specifics, modification_time,
                               std::move(unique_position));

  SetPossiblyTrimmedBaseSpecifics(std::move(trimmed_specifics));

  if (!data.creation_time.is_null()) {
    SetCreationTime(data.creation_time);
  }

  // Collaboration metadata is updated only on creation (i.e. for the first
  // time). Only `last_updated` field can be changed on local updates.
  if (data.collaboration_metadata.has_value()) {
    if (!metadata_.has_collaboration()) {
      metadata_.mutable_collaboration()->set_collaboration_id(
          data.collaboration_metadata->collaboration_id().value());
      metadata_.mutable_collaboration()
          ->mutable_creation_attribution()
          ->set_obfuscated_gaia_id(
              data.collaboration_metadata->created_by().ToString());
    }
    metadata_.mutable_collaboration()
        ->mutable_last_update_attribution()
        ->set_obfuscated_gaia_id(
            data.collaboration_metadata->last_updated_by().ToString());

    // Collaboration ID must never change.
    CHECK_EQ(metadata_.collaboration().collaboration_id(),
             data.collaboration_metadata->collaboration_id().value());
  }
}

void ProcessorEntityMetadata::RecordLocalDeletion(
    const DeletionOrigin& origin) {
  IncrementSequenceNumber();
  metadata_.set_modification_time(TimeToProtoTime(base::Time::Now()));
  metadata_.set_is_deleted(true);
  metadata_.clear_specifics_hash();
  metadata_.clear_bookmark_favicon_hash();
  metadata_.clear_possibly_trimmed_base_specifics();
  metadata_.clear_unique_position();

  if (origin.is_specified()) {
    *metadata_.mutable_deletion_origin() =
        origin.ToProto(version_info::GetVersionNumber());
  }

  metadata_.set_deleted_by_version(
      std::string(version_info::GetVersionNumber()));
}

void ProcessorEntityMetadata::SetPossiblyTrimmedBaseSpecifics(
    sync_pb::EntitySpecifics specifics) {
  *metadata_.mutable_possibly_trimmed_base_specifics() = std::move(specifics);
}

void ProcessorEntityMetadata::SetCreationTime(base::Time time) {
  metadata_.set_creation_time(TimeToProtoTime(time));
}

}  // namespace syncer
