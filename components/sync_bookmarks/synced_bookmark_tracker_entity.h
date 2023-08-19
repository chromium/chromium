// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_ENTITY_H_
#define COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_ENTITY_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace sync_pb {
class EntitySpecifics;
}  // namespace sync_pb

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace sync_bookmarks {

// This class manages the metadata corresponding to an individual BookmarkNode
// instance. It is analogous to the more generic syncer::ProcessorEntity, which
// is not reused for bookmarks for historic reasons.
class SyncedBookmarkTrackerEntity {
 public:
  // |bookmark_node| can be null for tombstones.
  SyncedBookmarkTrackerEntity(const bookmarks::BookmarkNode* bookmark_node,
                              sync_pb::EntityMetadata metadata);
  SyncedBookmarkTrackerEntity(const SyncedBookmarkTrackerEntity&) = delete;
  SyncedBookmarkTrackerEntity(SyncedBookmarkTrackerEntity&&) = delete;
  ~SyncedBookmarkTrackerEntity();

  SyncedBookmarkTrackerEntity& operator=(const SyncedBookmarkTrackerEntity&) =
      delete;
  SyncedBookmarkTrackerEntity& operator=(SyncedBookmarkTrackerEntity&&) =
      delete;

  // Returns true if this data is out of sync with the server.
  // A commit may or may not be in progress at this time.
  bool IsUnsynced() const;

  // Check whether |data| matches the stored specifics hash. It also compares
  // parent information (which is included in specifics).
  bool MatchesData(const syncer::EntityData& data) const;

  // Check whether |specifics| matches the stored specifics_hash.
  bool MatchesSpecificsHash(const sync_pb::EntitySpecifics& specifics) const;

  // Check whether |favicon_png_bytes| matches the stored
  // bookmark_favicon_hash.
  bool MatchesFaviconHash(const std::string& favicon_png_bytes) const;

  // Returns null for tombstones.
  const bookmarks::BookmarkNode* bookmark_node() const {
    return bookmark_node_;
  }

  // Used in local deletions to mark and entity as a tombstone.
  void clear_bookmark_node() { bookmark_node_ = nullptr; }

  // Used when replacing a node in order to update its otherwise immutable
  // UUID.
  void set_bookmark_node(const bookmarks::BookmarkNode* bookmark_node) {
    bookmark_node_ = bookmark_node;
  }

  const sync_pb::EntityMetadata& metadata() const { return metadata_; }

  sync_pb::EntityMetadata* MutableMetadata() { return &metadata_; }

  bool commit_may_have_started() const { return commit_may_have_started_; }
  void set_commit_may_have_started(bool value) {
    commit_may_have_started_ = value;
  }

  syncer::ClientTagHash GetClientTagHash() const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  // Null for tombstones.
  raw_ptr<const bookmarks::BookmarkNode, AcrossTasksDanglingUntriaged>
      bookmark_node_;

  // Serializable Sync metadata.
  sync_pb::EntityMetadata metadata_;

  // Whether there could be a commit sent to the server for this entity. It's
  // used to protect against sending tombstones for entities that have never
  // been sent to the server. It's only briefly false between the time was
  // first added to the tracker until the first commit request is sent to the
  // server. The tracker sets it to true in the constructor because this code
  // path is only executed in production when loading from disk.
  bool commit_may_have_started_ = false;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_ENTITY_H_
