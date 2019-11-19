// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_
#define COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

namespace syncer {
struct EntityData;
}

namespace sync_bookmarks {

using NodeMetadataPair = std::pair<const bookmarks::BookmarkNode*,
                                   std::unique_ptr<sync_pb::EntityMetadata>>;

// This class is responsible for keeping the mapping between bookmark nodes in
// the local model and the server-side corresponding sync entities. It manages
// the metadata for its entities and caches entity data upon a local change
// until commit confirmation is received.
class SyncedBookmarkTracker {
 public:
  class Entity {
   public:
    // |bookmark_node| can be null for tombstones. |metadata| must not be null.
    Entity(const bookmarks::BookmarkNode* bookmark_node,
           std::unique_ptr<sync_pb::EntityMetadata> metadata);
    ~Entity();

    // Returns true if this data is out of sync with the server.
    // A commit may or may not be in progress at this time.
    bool IsUnsynced() const;

    // Check whether |data| matches the stored specifics hash. It ignores parent
    // information.
    bool MatchesDataIgnoringParent(const syncer::EntityData& data) const;

    // Check whether |specifics| matches the stored specifics_hash.
    bool MatchesSpecificsHash(const sync_pb::EntitySpecifics& specifics) const;

    // Returns null for tombstones.
    const bookmarks::BookmarkNode* bookmark_node() const {
      return bookmark_node_;
    }

    // Used in local deletions to mark and entity as a tombstone.
    void clear_bookmark_node() { bookmark_node_ = nullptr; }

    // Used when replacing a node in order to update its otherwise immutable
    // GUID.
    void set_bookmark_node(const bookmarks::BookmarkNode* bookmark_node) {
      bookmark_node_ = bookmark_node;
    }

    const sync_pb::EntityMetadata* metadata() const {
      // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
      // Should be removed after figuring out the reason for the crash.
      CHECK(metadata_);
      return metadata_.get();
    }
    sync_pb::EntityMetadata* metadata() {
      // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
      // Should be removed after figuring out the reason for the crash.
      CHECK(metadata_);
      return metadata_.get();
    }

    bool commit_may_have_started() const { return commit_may_have_started_; }
    void set_commit_may_have_started(bool value) {
      commit_may_have_started_ = value;
    }

    // Returns the estimate of dynamically allocated memory in bytes.
    size_t EstimateMemoryUsage() const;

   private:
    // Null for tombstones.
    const bookmarks::BookmarkNode* bookmark_node_;

    // Serializable Sync metadata.
    const std::unique_ptr<sync_pb::EntityMetadata> metadata_;

    // Whether there could be a commit sent to the server for this entity. It's
    // used to protect against sending tombstones for entities that have never
    // been sent to the server. It's only briefly false between the time was
    // first added to the tracker until the first commit request is sent to the
    // server. The tracker sets it to true in the constructor because this code
    // path is only executed in production when loading from disk.
    bool commit_may_have_started_ = false;

    DISALLOW_COPY_AND_ASSIGN(Entity);
  };

  // |model_type_state| must not be null. null nodes in |nodes_metadata| can be
  // used to represent local tombstones.
  SyncedBookmarkTracker(
      std::vector<NodeMetadataPair> nodes_metadata,
      std::unique_ptr<sync_pb::ModelTypeState> model_type_state);
  ~SyncedBookmarkTracker();

  // Checks the integrity of the |model_metadata|. It also verifies that the
  // contents of the |model_metadata| match the contents of |model|. It should
  // only be called if the initial sync has completed.
  static bool BookmarkModelMatchesMetadata(
      const bookmarks::BookmarkModel* model,
      const sync_pb::BookmarkModelMetadata& model_metadata);

  // Returns null if no entity is found.
  const Entity* GetEntityForSyncId(const std::string& sync_id) const;

  // Returns null if no entity is found.
  const SyncedBookmarkTracker::Entity* GetEntityForBookmarkNode(
      const bookmarks::BookmarkNode* node) const;

  // Adds an entry for the |sync_id| and the corresponding local bookmark node
  // and metadata in |sync_id_to_entities_map_|.
  void Add(const std::string& sync_id,
           const bookmarks::BookmarkNode* bookmark_node,
           int64_t server_version,
           base::Time creation_time,
           const sync_pb::UniquePosition& unique_position,
           const sync_pb::EntitySpecifics& specifics);

  // Updates an existing entry for the |sync_id| and the corresponding metadata
  // in |sync_id_to_entities_map_|.
  void Update(const std::string& sync_id,
              int64_t server_version,
              base::Time modification_time,
              const sync_pb::UniquePosition& unique_position,
              const sync_pb::EntitySpecifics& specifics);

  // Updates the server version of an existing entry for the |sync_id|.
  void UpdateServerVersion(const std::string& sync_id, int64_t server_version);

  // Marks an existing entry for |sync_id| that a commit request might have been
  // sent to the server.
  void MarkCommitMayHaveStarted(const std::string& sync_id);

  // This class maintains the order of calls to this method and the same order
  // is guaranteed when returning local changes in
  // GetEntitiesWithLocalChanges() as well as in BuildBookmarkModelMetadata().
  void MarkDeleted(const std::string& sync_id);

  // Removes the entry coressponding to the |sync_id| from
  // |sync_id_to_entities_map_|.
  void Remove(const std::string& sync_id);

  // Increment sequence number in the metadata for the entity with |sync_id|.
  // Tracker must contain a non-tombstone entity with server id = |sync_id|.
  void IncrementSequenceNumber(const std::string& sync_id);

  sync_pb::BookmarkModelMetadata BuildBookmarkModelMetadata() const;

  // Returns true if there are any local entities to be committed.
  bool HasLocalChanges() const;

  const sync_pb::ModelTypeState& model_type_state() const {
    return *model_type_state_;
  }

  void set_model_type_state(
      std::unique_ptr<sync_pb::ModelTypeState> model_type_state) {
    model_type_state_ = std::move(model_type_state);
  }

  std::vector<const Entity*> GetAllEntities() const;

  std::vector<const Entity*> GetEntitiesWithLocalChanges(
      size_t max_entries) const;

  // Updates the tracker after receiving the commit response. |old_id| should be
  // equal to |new_id| for all updates except the initial commit, where the
  // temporary client-generated ID will be overriden by the server-provided
  // final ID. In which case |sync_id_to_entities_map_| will be updated
  // accordingly.
  void UpdateUponCommitResponse(const std::string& old_id,
                                const std::string& new_id,
                                int64_t acked_sequence_number,
                                int64_t server_version);

  // Informs the tracker that the sync id for an entity has changed. It updates
  // the internal state of the tracker accordingly.
  void UpdateSyncForLocalCreationIfNeeded(const std::string& old_id,
                                          const std::string& new_id);

  // Informs the tracker that a BookmarkNode has been replaced. It updates
  // the internal state of the tracker accordingly.
  void UpdateBookmarkNodePointer(const bookmarks::BookmarkNode* old_node,
                                 const bookmarks::BookmarkNode* new_node);

  // Set the value of |EntityMetadata.acked_sequence_number| in the entity with
  // |sync_id| to be equal to |EntityMetadata.sequence_number| such that it is
  // not returned in GetEntitiesWithLocalChanges().
  void AckSequenceNumber(const std::string& sync_id);

  // Whether the tracker is empty or not.
  bool IsEmpty() const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  // Returns number of tracked entities. Used only in test.
  size_t TrackedEntitiesCountForTest() const;

  // Returns number of tracked bookmarks that aren't deleted.
  size_t TrackedBookmarksCountForDebugging() const;

  // Returns number of bookmarks that have been deleted but the server hasn't
  // confirmed the deletion yet.
  size_t TrackedUncommittedTombstonesCountForDebugging() const;

  // Checks whther all nodes in |bookmark_model| that *should* be tracked as per
  // CanSyncNode() are tracked.
  void CheckAllNodesTracked(
      const bookmarks::BookmarkModel* bookmark_model) const;

 private:
  // Returns null if no entity is found.
  Entity* GetMutableEntityForSyncId(const std::string& sync_id);

  // Reorders |entities| that represents local non-deletions such that parent
  // creation/update is before child creation/update. Returns the ordered list.
  std::vector<const Entity*> ReorderUnsyncedEntitiesExceptDeletions(
      const std::vector<const Entity*>& entities) const;

  // Recursive method that starting from |node| appends all corresponding
  // entities with updates in top-down order to |ordered_entities|.
  void TraverseAndAppend(const bookmarks::BookmarkNode* node,
                         std::vector<const SyncedBookmarkTracker::Entity*>*
                             ordered_entities) const;

  // A map of sync server ids to sync entities. This should contain entries and
  // metadata for almost everything.
  std::map<std::string, std::unique_ptr<Entity>> sync_id_to_entities_map_;

  // A map of bookmark nodes to sync entities. It's keyed by the bookmark node
  // pointers which get assigned when loading the bookmark model. This map is
  // first initialized in the constructor.
  std::map<const bookmarks::BookmarkNode*, Entity*>
      bookmark_node_to_entities_map_;

  // A list of pending local bookmark deletions. They should be sent to the
  // server in the same order as stored in the list. The same order should also
  // be maintained across browser restarts (i.e. across calls to the ctor() and
  // BuildBookmarkModelMetadata().
  std::vector<Entity*> ordered_local_tombstones_;

  // The model metadata (progress marker, initial sync done, etc).
  std::unique_ptr<sync_pb::ModelTypeState> model_type_state_;

  DISALLOW_COPY_AND_ASSIGN(SyncedBookmarkTracker);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_
