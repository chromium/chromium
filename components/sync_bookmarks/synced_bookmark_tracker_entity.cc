// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"

#include <utility>

#include "base/check.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace sync_bookmarks {

SyncedBookmarkTrackerEntity::SyncedBookmarkTrackerEntity(
    const bookmarks::BookmarkNode* bookmark_node,
    syncer::ProcessorEntityMetadata entity_metadata)
    : bookmark_node_(bookmark_node), metadata_(std::move(entity_metadata)) {
  if (bookmark_node_) {
    DCHECK(!metadata_.IsDeleted());
  } else {
    DCHECK(metadata_.IsDeleted());
  }
}

SyncedBookmarkTrackerEntity::~SyncedBookmarkTrackerEntity() = default;

bool SyncedBookmarkTrackerEntity::IsDeleted() const {
  return metadata_.IsDeleted();
}

bool SyncedBookmarkTrackerEntity::IsUnsynced() const {
  return metadata_.IsUnsynced();
}

bool SyncedBookmarkTrackerEntity::IsUnsyncedLocalCreation() const {
  return metadata_.IsUnsyncedLocalCreation();
}

bool SyncedBookmarkTrackerEntity::IsVersionAlreadyKnown(
    int64_t update_version) const {
  return metadata_.IsVersionAlreadyKnown(update_version);
}

bool SyncedBookmarkTrackerEntity::MatchesData(
    const syncer::EntityData& data) const {
  return metadata_.MatchesData(data);
}

bool SyncedBookmarkTrackerEntity::MatchesBaseData(
    const syncer::EntityData& data) const {
  return metadata_.MatchesBaseData(data);
}

bool SyncedBookmarkTrackerEntity::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  return metadata_.MatchesSpecificsHash(specifics);
}

bool SyncedBookmarkTrackerEntity::MatchesFaviconHash(
    const std::string& favicon_png_bytes) const {
  return metadata_.MatchesFaviconHash(favicon_png_bytes);
}

syncer::ClientTagHash SyncedBookmarkTrackerEntity::GetClientTagHash() const {
  return metadata_.GetClientTagHash();
}

size_t SyncedBookmarkTrackerEntity::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = metadata_.EstimateMemoryUsage();
  memory_usage += sizeof(bookmark_node_);
  return memory_usage;
}

void SyncedBookmarkTrackerEntity::RecordAcceptedRemoteUpdate(
    const syncer::UpdateResponseData& update) {
  std::optional<sync_pb::UniquePosition> unique_position;
  if (update.entity.specifics.bookmark().has_unique_position()) {
    unique_position = update.entity.specifics.bookmark().unique_position();
  }
  metadata_.RecordAcceptedRemoteUpdate(
      update, /*trimmed_specifics=*/sync_pb::EntitySpecifics(),
      std::move(unique_position));
}

void SyncedBookmarkTrackerEntity::RecordForcedRemoteUpdate(
    const syncer::UpdateResponseData& update) {
  std::optional<sync_pb::UniquePosition> unique_position;
  if (update.entity.specifics.bookmark().has_unique_position()) {
    unique_position = update.entity.specifics.bookmark().unique_position();
  }
  metadata_.RecordForcedRemoteUpdate(
      update, /*trimmed_specifics=*/sync_pb::EntitySpecifics(),
      std::move(unique_position));
}

void SyncedBookmarkTrackerEntity::RecordIgnoredRemoteUpdate(
    const syncer::UpdateResponseData& update) {
  metadata_.RecordIgnoredRemoteUpdate(update);
}

void SyncedBookmarkTrackerEntity::OverrideServerMetadata(
    const std::string& server_id,
    int64_t server_version) {
  metadata_.OverrideServerMetadata(server_id, server_version);
}

void SyncedBookmarkTrackerEntity::RecordLocalUpdate(
    const sync_pb::EntitySpecifics& specifics,
    base::Time modification_time) {
  CHECK(!IsDeleted());
  metadata_.UpdateMetadataForLocalUpdate(
      specifics, modification_time, specifics.bookmark().unique_position());
}

void SyncedBookmarkTrackerEntity::RecordCommitResponse(
    const syncer::CommitResponseData& ack) {
  metadata_.RecordCommitResponse(ack);
}

void SyncedBookmarkTrackerEntity::RecordLocalDeletion(
    PassKey,
    const syncer::DeletionOrigin& origin) {
  metadata_.RecordLocalDeletion(origin);
}

void SyncedBookmarkTrackerEntity::IncrementSequenceNumber() {
  metadata_.IncrementSequenceNumber();
}

void SyncedBookmarkTrackerEntity::UndeleteTombstoneForBookmarkNode(
    PassKey,
    const bookmarks::BookmarkNode* node,
    const sync_pb::EntitySpecifics& specifics,
    base::Time modification_time) {
  DCHECK(node);
  DCHECK(IsDeleted());
  bookmark_node_ = node;
  metadata_.UpdateMetadataForLocalUpdate(
      specifics, modification_time, specifics.bookmark().unique_position());
}

}  // namespace sync_bookmarks
