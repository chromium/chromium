// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_
#define COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace sync_pb {
class BookmarkModelMetadata;
class EntitySpecifics;
}  // namespace sync_pb

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace sync_bookmarks {

class BookmarkModelView;
class SyncedBookmarkTrackerEntity;

// This class is responsible for keeping the mapping between bookmark nodes in
// the local model and the server-side corresponding sync entities. It manages
// the metadata for its entities and caches entity data upon a local change
// until commit confirmation is received.
class SyncedBookmarkTracker {
 public:
  // Returns a client tag hash given a bookmark UUID.
  static syncer::ClientTagHash GetClientTagHashFromUuid(const base::Uuid& uuid);

  // Creates an empty instance with no entities. Never returns null.
  static std::unique_ptr<SyncedBookmarkTracker> CreateEmpty(
      sync_pb::DataTypeState data_type_state);

  // Loads a tracker from a proto (usually from disk) after enforcing the
  // consistency of the metadata against the BookmarkModel. Returns null if the
  // data is inconsistent with sync metadata (i.e. corrupt). `model` must not be
  // null.
  static std::unique_ptr<SyncedBookmarkTracker>
  CreateFromBookmarkModelAndMetadata(
      const BookmarkModelView* model,
      sync_pb::BookmarkModelMetadata model_metadata);

  SyncedBookmarkTracker(const SyncedBookmarkTracker&) = delete;
  SyncedBookmarkTracker& operator=(const SyncedBookmarkTracker&) = delete;

  ~SyncedBookmarkTracker();

  // This method is used to denote that all bookmarks are reuploaded and there
  // is no need to reupload them again after next browser startup.
  void SetBookmarksReuploaded();

  // Returns null if no entity is found.
  const SyncedBookmarkTrackerEntity* GetEntityForSyncId(
      const std::string& sync_id) const;

  // Returns null if no entity is found.
  const SyncedBookmarkTrackerEntity* GetEntityForClientTagHash(
      const syncer::ClientTagHash& client_tag_hash) const;

  // Convenience function, similar to GetEntityForClientTagHash().
  const SyncedBookmarkTrackerEntity* GetEntityForUuid(
      const base::Uuid& uuid) const;

  // Returns null if no entity is found.
  const SyncedBookmarkTrackerEntity* GetEntityForBookmarkNode(
      const bookmarks::BookmarkNode* node) const;

  // Starts tracking local bookmark `bookmark_node`, which must not be tracked
  // beforehand. The rest of the arguments represent the initial metadata.
  // Returns the tracked entity.
  const SyncedBookmarkTrackerEntity* Add(
      const bookmarks::BookmarkNode* bookmark_node,
      const std::string& sync_id,
      int64_t server_version,
      base::Time creation_time,
      const sync_pb::EntitySpecifics& specifics);

  // Updates the sync metadata for a tracked entity. `entity` must be owned by
  // this tracker.
  void Update(const SyncedBookmarkTrackerEntity* entity,
              int64_t server_version,
              base::Time modification_time,
              const sync_pb::EntitySpecifics& specifics);

  // Updates the server version of an existing entity. `entity` must be owned by
  // this tracker.
  void UpdateServerVersion(const SyncedBookmarkTrackerEntity* entity,
                           int64_t server_version);

  // Marks an existing entry that a commit request might have been sent to the
  // server. `entity` must be owned by this tracker.
  void MarkCommitMayHaveStarted(const SyncedBookmarkTrackerEntity* entity);

  // This class maintains the order of calls to this method and the same order
  // is guaranteed when returning local changes in
  // GetEntitiesWithLocalChanges() as well as in BuildBookmarkModelMetadata().
  // `entity` must be owned by this tracker. `location` is used to propagate
  // debug information about which piece of code triggered the deletion.
  void MarkDeleted(const SyncedBookmarkTrackerEntity* entity,
                   const base::Location& location);

  // Untracks an entity, which also invalidates the pointer. `entity` must be
  // owned by this tracker.
  void Remove(const SyncedBookmarkTrackerEntity* entity);

  // Increment sequence number in the metadata for `entity`. `entity` must be
  // owned by this tracker.
  void IncrementSequenceNumber(const SyncedBookmarkTrackerEntity* entity);

  sync_pb::BookmarkModelMetadata BuildBookmarkModelMetadata() const;

  // Returns true if there are any local entities to be committed.
  bool HasLocalChanges() const;

  const sync_pb::DataTypeState& data_type_state() const {
    return data_type_state_;
  }

  void set_data_type_state(sync_pb::DataTypeState data_type_state) {
    data_type_state_ = std::move(data_type_state);
  }

  std::vector<const SyncedBookmarkTrackerEntity*> GetAllEntities() const;

  std::vector<const SyncedBookmarkTrackerEntity*> GetEntitiesWithLocalChanges()
      const;

  // Updates the tracker after receiving the commit response. `sync_id` should
  // match the already tracked sync ID for `entity`, with the exception of the
  // initial commit, where the temporary client-generated ID will be overridden
  // by the server-provided final ID. `entity` must be owned by this tracker.
  void UpdateUponCommitResponse(const SyncedBookmarkTrackerEntity* entity,
                                const std::string& sync_id,
                                int64_t server_version,
                                int64_t acked_sequence_number);

  // Informs the tracker that the sync ID for `entity` has changed. It updates
  // the internal state of the tracker accordingly. `entity` must be owned by
  // this tracker.
  void UpdateSyncIdIfNeeded(const SyncedBookmarkTrackerEntity* entity,
                            const std::string& sync_id);

  // Used to start tracking an entity that overwrites a previous local tombstone
  // (e.g. user-initiated bookmark deletion undo). `entity` must be owned by
  // this tracker.
  void UndeleteTombstoneForBookmarkNode(
      const SyncedBookmarkTrackerEntity* entity,
      const bookmarks::BookmarkNode* node);

  // Set the value of `EntityMetadata.acked_sequence_number` for `entity` to be
  // equal to `EntityMetadata.sequence_number` such that it is not returned in
  // GetEntitiesWithLocalChanges(). `entity` must be owned by this tracker.
  void AckSequenceNumber(const SyncedBookmarkTrackerEntity* entity);

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

  // Clears the specifics hash for `entity`, useful for testing.
  void ClearSpecificsHashForTest(const SyncedBookmarkTrackerEntity* entity);

  // Checks whether all nodes in `bookmark_model` that *should* be tracked as
  // per IsNodeSyncable() are tracked.
  void CheckAllNodesTracked(const BookmarkModelView* bookmark_model) const;

  // This method is used to mark all entities except permanent nodes as
  // unsynced. This will cause reuploading of all bookmarks. The reupload
  // will be initiated only when the `bookmarks_hierarchy_fields_reuploaded`
  // field in BookmarksMetadata is false. This field is used to prevent
  // reuploading after each browser restart. Returns true if the reupload was
  // initiated.
  // TODO(crbug.com/40780588): remove this code when most of bookmarks are
  // reuploaded.
  bool ReuploadBookmarksOnLoadIfNeeded();

  // Causes the tracker to remember that a remote sync update (initial or
  // incremental) was ignored because its parent was unknown (either because
  // the data was corrupt or because the update is a descendant of an
  // unsupported permanent folder).
  void RecordIgnoredServerUpdateDueToMissingParent(int64_t server_version);

  std::optional<int64_t> GetNumIgnoredUpdatesDueToMissingParentForTest() const;
  std::optional<int64_t>
  GetMaxVersionAmongIgnoredUpdatesDueToMissingParentForTest() const;

 private:
  // Enumeration of possible reasons why persisted metadata are considered
  // corrupted and don't match the bookmark model. Used in UMA metrics. Do not
  // re-order or delete these entries; they are used in a UMA histogram. Please
  // edit SyncBookmarkModelMetadataCorruptionReason in enums.xml if a value is
  // added.
  // LINT.IfChange(SyncBookmarkModelMetadataCorruptionReason)
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
    BOOKMARK_UUID_MISMATCH = 9,
    DUPLICATED_CLIENT_TAG_HASH = 10,
    TRACKED_MANAGED_NODE = 11,
    MISSING_CLIENT_TAG_HASH = 12,
    MISSING_FAVICON_HASH = 13,

    kMaxValue = MISSING_FAVICON_HASH
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncBookmarkModelMetadataCorruptionReason)

  SyncedBookmarkTracker(
      sync_pb::DataTypeState data_type_state,
      bool bookmarks_reuploaded,
      std::optional<int64_t> num_ignored_updates_due_to_missing_parent,
      std::optional<int64_t>
          max_version_among_ignored_updates_due_to_missing_parent);

  // Add entities to `this` tracker based on the content of `*model` and
  // `model_metadata`. Validates the integrity of `*model` and `model_metadata`
  // and returns an enum representing any inconsistency.
  CorruptionReason InitEntitiesFromModelAndMetadata(
      const BookmarkModelView* model,
      sync_pb::BookmarkModelMetadata model_metadata);

  // Conceptually, find a tracked entity that matches `entity` and returns a
  // non-const pointer of it. The actual implementation is a const_cast.
  // `entity` must be owned by this tracker.
  SyncedBookmarkTrackerEntity* AsMutableEntity(
      const SyncedBookmarkTrackerEntity* entity);

  // Reorders `entities` that represents local non-deletions such that parent
  // creation/update is before child creation/update. Returns the ordered list.
  std::vector<const SyncedBookmarkTrackerEntity*>
  ReorderUnsyncedEntitiesExceptDeletions(
      const std::vector<const SyncedBookmarkTrackerEntity*>& entities) const;

  // Recursive method that starting from `node` appends all corresponding
  // entities with updates in top-down order to `ordered_entities`.
  void TraverseAndAppend(
      const bookmarks::BookmarkNode* node,
      std::vector<const SyncedBookmarkTrackerEntity*>* ordered_entities) const;

  // A map of sync server ids to sync entities. This should contain entries and
  // metadata for almost everything.
  std::unordered_map<std::string, std::unique_ptr<SyncedBookmarkTrackerEntity>>
      sync_id_to_entities_map_;

  // Index for efficient lookups by client tag hash.
  std::unordered_map<
      syncer::ClientTagHash,
      raw_ptr<const SyncedBookmarkTrackerEntity, CtnExperimental>,
      syncer::ClientTagHash::Hash>
      client_tag_hash_to_entities_map_;

  // A map of bookmark nodes to sync entities. It's keyed by the bookmark node
  // pointers which get assigned when loading the bookmark model. This map is
  // first initialized in the constructor.
  std::unordered_map<const bookmarks::BookmarkNode*,
                     raw_ptr<SyncedBookmarkTrackerEntity, CtnExperimental>>
      bookmark_node_to_entities_map_;

  // A list of pending local bookmark deletions. They should be sent to the
  // server in the same order as stored in the list. The same order should also
  // be maintained across browser restarts (i.e. across calls to the ctor() and
  // BuildBookmarkModelMetadata().
  std::vector<raw_ptr<SyncedBookmarkTrackerEntity, VectorExperimental>>
      ordered_local_tombstones_;

  // The model metadata (progress marker, initial sync done, etc).
  sync_pb::DataTypeState data_type_state_;

  // This field contains the value of
  // BookmarksMetadata::bookmarks_hierarchy_fields_reuploaded.
  // TODO(crbug.com/40780588): remove this code when most of bookmarks are
  // reuploaded.
  bool bookmarks_reuploaded_ = false;

  // See corresponding proto fields in BookmarkModelMetadata.
  std::optional<int64_t> num_ignored_updates_due_to_missing_parent_;
  std::optional<int64_t>
      max_version_among_ignored_updates_due_to_missing_parent_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_
