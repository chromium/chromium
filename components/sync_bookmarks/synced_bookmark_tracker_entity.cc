// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"

#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace sync_bookmarks {

namespace {

void HashSpecifics(const sync_pb::EntitySpecifics& specifics,
                   std::string* hash) {
  DCHECK_GT(specifics.ByteSize(), 0);
  *hash =
      base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()));
}

}  // namespace

SyncedBookmarkTrackerEntity::SyncedBookmarkTrackerEntity(
    const bookmarks::BookmarkNode* bookmark_node,
    sync_pb::EntityMetadata metadata)
    : bookmark_node_(bookmark_node), metadata_(std::move(metadata)) {
  if (bookmark_node) {
    DCHECK(!metadata_.is_deleted());
  } else {
    DCHECK(metadata_.is_deleted());
  }
}

SyncedBookmarkTrackerEntity::~SyncedBookmarkTrackerEntity() = default;

bool SyncedBookmarkTrackerEntity::IsUnsynced() const {
  return metadata_.sequence_number() > metadata_.acked_sequence_number();
}

bool SyncedBookmarkTrackerEntity::MatchesData(
    const syncer::EntityData& data) const {
  if (metadata_.is_deleted() || data.is_deleted()) {
    // In case of deletion, no need to check the specifics.
    return metadata_.is_deleted() == data.is_deleted();
  }
  return MatchesSpecificsHash(data.specifics);
}

bool SyncedBookmarkTrackerEntity::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  DCHECK(!metadata_.is_deleted());
  DCHECK_GT(specifics.ByteSize(), 0);
  std::string hash;
  HashSpecifics(specifics, &hash);
  return hash == metadata_.specifics_hash();
}

bool SyncedBookmarkTrackerEntity::MatchesFaviconHash(
    const std::string& favicon_png_bytes) const {
  DCHECK(!metadata_.is_deleted());
  return metadata_.bookmark_favicon_hash() ==
         base::PersistentHash(favicon_png_bytes);
}

syncer::ClientTagHash SyncedBookmarkTrackerEntity::GetClientTagHash() const {
  return syncer::ClientTagHash::FromHashed(metadata_.client_tag_hash());
}

size_t SyncedBookmarkTrackerEntity::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  // Include the size of the pointer to the bookmark node.
  memory_usage += sizeof(bookmark_node_);
  memory_usage += EstimateMemoryUsage(metadata_);
  return memory_usage;
}

}  // namespace sync_bookmarks
