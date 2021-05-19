// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_
#define COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace base {
class GUID;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace syncer {
class ClientTagHash;
struct EntityData;
class UniquePosition;
}  // namespace syncer

namespace sync_bookmarks {

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
    // GUID.
    void set_bookmark_node(const bookmarks::BookmarkNode* bookmark_node) {
      bookmark_node_ = bookmark_node;
    }

    const sync_pb::EntityMetadata* metadata() const {
      return metadata_.get();
    }

    sync_pb::EntityMetadata* metadata() {
      return metadata_.get();
    }

    bool commit_may_have_started() const { return commit_may_have_started_; }
    void set_commit_may_have_started(bool value) {
      commit_may_have_started_ = value;
    }

    void PopulateFaviconHashIfUnset(const std::string& favicon_png_bytes);

    syncer::ClientTagHash GetClientTagHash() const;

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

  // Returns a client tag hash given a bookmark GUID.
  static syncer::ClientTagHash GetClientTagHashFromGUID(const base::GUID& guid);

  // Creates an empty instance with no entities. Never returns null.
  static std::unique_ptr<SyncedBookmarkTracker> CreateEmpty(
      sync_pb::ModelTypeState model_type_state);

  // Loads a tracker from a proto (usually from disk) after enforcing the
  // consistency of the metadata against the BookmarkModel. Returns null if the
  // data is inconsistent with sync metadata (i.e. corrupt). |model| must not be
  // null.
  static std::unique_ptr<SyncedBookmarkTracker>
  CreateFromBookmarkModelAndMetadata(
      const bookmarks::BookmarkModel* model,
      sync_pb::BookmarkModelMetadata model_metadata);

  ~SyncedBookmarkTracker();

  // This method is used to denote that all bookmarks are reuploaded and there
  // is no need to reupload them again after next browser startup.
  void SetBookmarksFullTitleReuploaded();

  // Returns null if no entity is found.
  const Entity* GetEntityForSyncId(const std::string& sync_id) const;

  // Returns null if no entity is found.
  const Entity* GetEntityForClientTagHash(
      const syncer::ClientTagHash& client_tag_hash) const;

  // Returns null if no entity is found.
  const SyncedBookmarkTracker::Entity* GetEntityForBookmarkNode(
      const bookmarks::BookmarkNode* node) const;

  // Starts tracking local bookmark |bookmark_node|, which must not be tracked
  // beforehand. The rest of the arguments represent the initial metadata.
  // Returns the tracked entity.
  const Entity* Add(const bookmarks::BookmarkNode* bookmark_node,
                    const std::string& sync_id,
                    int64_t server_version,
                    base::Time creation_time,
                    const syncer::UniquePosition& unique_position,
                    const sync_pb::EntitySpecifics& specifics);

  // Updates the sync metadata for a tracked entity. |entity| must be owned by
  // this tracker.
  void Update(const Entity* entity,
              int64_t server_version,
              base::Time modification_time,
              const syncer::UniquePosition& unique_position,
              const sync_pb::EntitySpecifics& specifics);

  // Updates the server version of an existing entity. |entity| must be owned by
  // this tracker.
  void UpdateServerVersion(const Entity* entity, int64_t server_version);

  // Populates the metadata field representing the hashed favicon. This method
  // is effectively used to backfill the proto field, which was introduced late.
  void PopulateFaviconHashIfUnset(const Entity* entity,
                                  const std::string& favicon_png_bytes);

  // Marks an existing entry that a commit request might have been sent to the
  // server. |entity| must be owned by this tracker.
  void MarkCommitMayHaveStarted(const Entity* entity);

  // This class maintains the order of calls to this method and the same order
  // is guaranteed when returning local changes in
  // GetEntitiesWithLocalChanges() as well as in BuildBookmarkModelMetadata().
  // |entity| must be owned by this tracker.
  void MarkDeleted(const Entity* entity);

  // Untracks an entity, which also invalidates the pointer. |entity| must be
  // owned by this tracker.
  void Remove(const Entity* entity);

  // Increment sequence number in the metadata for |entity|. |entity| must be
  // owned by this tracker.
  void IncrementSequenceNumber(const Entity* entity);

  sync_pb::BookmarkModelMetadata BuildBookmarkModelMetadata() const;

  // Returns true if there are any local entities to be committed.
  bool HasLocalChanges() const;

  const sync_pb::ModelTypeState& model_type_state() const {
    return model_type_state_;
  }

  void set_model_type_state(sync_pb::ModelTypeState model_type_state) {
    model_type_state_ = std::move(model_type_state);
  }

  // Treats the current time as last sync time.
  // TODO(crbug.com/1032052): Remove this code once all local sync metadata is
  // required to populate the client tag (and be considered invalid otherwise).
  void UpdateLastSyncTime() { last_sync_time_ = base::Time::Now(); }

  std::vector<const Entity*> GetAllEntities() const;

  std::vector<const Entity*> GetEntitiesWithLocalChanges(
      size_t max_entries) const;

  // Updates the tracker after receiving the commit response. |sync_id| should
  // match the already tracked sync ID for |entity|, with the exception of the
  // initial commit, where the temporary client-generated ID will be overridden
  // by the server-provided final ID. |entity| must be owned by this tracker.
  void UpdateUponCommitResponse(const Entity* entity,
                                const std::string& sync_id,
                                int64_t server_version,
                                int64_t acked_sequence_number);

  // Informs the tracker that the sync ID for |entity| has changed. It updates
  // the internal state of the tracker accordingly. |entity| must be owned by
  // this tracker.
  void UpdateSyncIdForLocalCreationIfNeeded(const Entity* entity,
                                            const std::string& sync_id);

  // Used to start tracking an entity that overwrites a previous local tombstone
  // (e.g. user-initiated bookmark deletion undo). |entity| must be owned by
  // this tracker.
  void UndeleteTombstoneForBookmarkNode(const Entity* entity,
                                        const bookmarks::BookmarkNode* node);

  // Set the value of |EntityMetadata.acked_sequence_number| for |entity| to be
  // equal to |EntityMetadata.sequence_number| such that it is not returned in
  // GetEntitiesWithLocalChanges(). |entity| must be owned by this tracker.
  void AckSequenceNumber(const Entity* entity);

  // Whether the tracker is empty or not.
  bool IsEmpty() const;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  // Returns number of tracked bookmarks that aren't deleted.
  size_t TrackedBookmarksCount() const;

  // Returns number of bookmarks that have been deleted but the server hasn't
  // confirmed the deletion yet.
  size_t TrackedUncommittedTombstonesCount() const;

  // Returns number of tracked entities. Used only in test.
  size_t TrackedEntitiesCountForTest() const;

  // Clears the specifics hash for |entity|, useful for testing.
  void ClearSpecificsHashForTest(const Entity* entity);

  // Checks whther all nodes in |bookmark_model| that *should* be tracked as per
  // CanSyncNode() are tracked.
  void CheckAllNodesTracked(
      const bookmarks::BookmarkModel* bookmark_model) const;

  // This method is used to mark all entities except permanent nodes as
  // unsynced. This will cause reuploading of all bookmarks. The reupload
  // will be initiated only when the |bookmarks_full_title_reuploaded| field in
  // BookmarksMetadata is false. This field is used to prevent reuploading after
  // each browser restart. Returns true if the reupload was initiated.
  // TODO(crbug.com/1066962): remove this code when most of bookmarks are
  // reuploaded.
  bool ReuploadBookmarksOnLoadIfNeeded();

  // Returns whether bookmark commits sent to the server (most importantly
  // creations) should populate client tags.
  bool bookmark_client_tags_in_protocol_enabled() const;

 private:
  // Enumeration of possible reasons why persisted metadata are considered
  // corrupted and don't match the bookmark model. Used in UMA metrics. Do not
  // re-order or delete these entries; they are used in a UMA histogram. Please
  // edit SyncBookmarkModelMetadataCorruptionReason in enums.xml if a value is
  // added.
  enum class CorruptionReason {
    NO_CORRUPTION = 0,
    MISSING_SERVER_ID = 1,
    BOOKMARK_ID_IN_TOMBSTONE = 2,
    MISSING_BOOKMARK_ID = 3,
    // COUNT_MISMATCH = 4,  // Deprecated.
    // IDS_MISMATCH = 5,  // Deprecated.
    DUPLICATED_SERVER_ID = 6,
    UNKNOWN_BOOKMARK_ID = 7,
    UNTRACKED_BOOKMARK = 8,
    BOOKMARK_GUID_MISMATCH = 9,
    DUPLICATED_CLIENT_TAG_HASH = 10,
    TRACKED_MANAGED_NODE = 11,
    MISSING_CLIENT_TAG_HASH = 12,

    kMaxValue = MISSING_CLIENT_TAG_HASH
  };

  SyncedBookmarkTracker(sync_pb::ModelTypeState model_type_state,
                        bool bookmarks_full_title_reuploaded,
                        base::Time last_sync_time);

  // Add entities to |this| tracker based on the content of |*model| and
  // |model_metadata|. Validates the integrity of |*model| and |model_metadata|
  // and returns an enum representing any inconsistency.
  CorruptionReason InitEntitiesFromModelAndMetadata(
      const bookmarks::BookmarkModel* model,
      sync_pb::BookmarkModelMetadata model_metadata);

  // Conceptually, find a tracked entity that matches |entity| and returns a
  // non-const pointer of it. The actual implementation is a const_cast.
  // |entity| must be owned by this tracker.
  Entity* AsMutableEntity(const Entity* entity);

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
  std::unordered_map<std::string, std::unique_ptr<Entity>>
      sync_id_to_entities_map_;

  // Index for efficient lookups by client tag hash.
  std::unordered_map<syncer::ClientTagHash,
                     const Entity*,
                     syncer::ClientTagHash::Hash>
      client_tag_hash_to_entities_map_;

  // A map of bookmark nodes to sync entities. It's keyed by the bookmark node
  // pointers which get assigned when loading the bookmark model. This map is
  // first initialized in the constructor.
  std::unordered_map<const bookmarks::BookmarkNode*, Entity*>
      bookmark_node_to_entities_map_;

  // A list of pending local bookmark deletions. They should be sent to the
  // server in the same order as stored in the list. The same order should also
  // be maintained across browser restarts (i.e. across calls to the ctor() and
  // BuildBookmarkModelMetadata().
  std::vector<Entity*> ordered_local_tombstones_;

  // The model metadata (progress marker, initial sync done, etc).
  sync_pb::ModelTypeState model_type_state_;

  // This field contains the value of
  // BookmarksMetadata::bookmarks_full_title_reuploaded.
  // TODO(crbug.com/1066962): remove this code when most of bookmarks are
  // reuploaded.
  bool bookmarks_full_title_reuploaded_ = false;

  // The local timestamp corresponding to the last time remote updates were
  // received.
  // TODO(crbug.com/1032052): Remove this code once all local sync metadata is
  // required to populate the client tag (and be considered invalid otherwise).
  base::Time last_sync_time_;

  // Represents whether bookmark commits sent to the server (most importantly
  // creations) populate client tags.
  // TODO(crbug.com/1032052): remove this code when the logic is enabled by
  // default and enforced to true upon startup.
  bool bookmark_client_tags_in_protocol_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(SyncedBookmarkTracker);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_
