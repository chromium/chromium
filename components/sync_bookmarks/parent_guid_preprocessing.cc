// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/parent_guid_preprocessing.h"

#include <memory>
#include <string_view>
#include <unordered_map>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"

namespace sync_bookmarks {

namespace {

// The tag used in the sync protocol to identity well-known permanent folders.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Fake GUID used to populate field |BookmarkSpecifics.parent_guid| for the case
// where a parent is specified in |SyncEntity.parent_id| but the parent's
// precise GUID could not be determined. Doing this is mostly relevant for UMA
// metrics. The precise GUID used in this string was generated using the same
// technique as the well-known GUIDs in bookmarks::BookmarkNode, using the name
// "unknown_parent_guid". The precise value is irrelevant though and can be
// changed since all updates using the parent GUID will be ignored in practice.
const char kInvalidParentGuid[] = "220a410e-37b9-5bbc-8674-ea982459f940";

bool NeedsParentGuidInSpecifics(const syncer::UpdateResponseData& update) {
  return !update.entity.is_deleted() &&
         update.entity.legacy_parent_id != std::string("0") &&
         update.entity.server_defined_unique_tag.empty() &&
         !update.entity.specifics.bookmark().has_parent_guid();
}

// Tried to use the information known by |tracker| to determine the GUID of the
// parent folder, for the entity updated in |update|. Returns an invalid GUID
// if the GUID could not be determined. |tracker| must not be null.
base::Uuid TryGetParentGuidFromTracker(
    const SyncedBookmarkTracker* tracker,
    const syncer::UpdateResponseData& update) {
  DCHECK(tracker);
  DCHECK(!update.entity.is_deleted());
  DCHECK(!update.entity.legacy_parent_id.empty());
  DCHECK(update.entity.server_defined_unique_tag.empty());
  DCHECK(!update.entity.specifics.bookmark().has_parent_guid());

  const SyncedBookmarkTrackerEntity* const tracked_parent =
      tracker->GetEntityForSyncId(update.entity.legacy_parent_id);
  if (!tracked_parent) {
    // Parent not known by tracker.
    return base::Uuid();
  }

  if (!tracked_parent->bookmark_node()) {
    // Parent is a tombstone; cannot determine the GUID.
    return base::Uuid();
  }

  return tracked_parent->bookmark_node()->uuid();
}

std::string_view GetGuidForEntity(const syncer::EntityData& entity) {
  // Special-case permanent folders, which may not include a GUID in specifics.
  if (entity.server_defined_unique_tag == kBookmarkBarTag) {
    return bookmarks::kBookmarkBarNodeUuid;
  }
  if (entity.server_defined_unique_tag == kOtherBookmarksTag) {
    return bookmarks::kOtherBookmarksNodeUuid;
  }
  if (entity.server_defined_unique_tag == kMobileBookmarksTag) {
    return bookmarks::kMobileBookmarksNodeUuid;
  }
  // Fall back to the regular case, i.e. GUID in specifics, or an empty value
  // if not present (including tombstones).
  return entity.specifics.bookmark().guid();
}

// Map from sync IDs (server-provided entity IDs) to GUIDs. The
// returned map uses std::string_view that rely on the lifetime of the strings
// in |updates|. |updates| must not be null.
class LazySyncIdToGuidMapInUpdates {
 public:
  // |updates| must not be null and must outlive this object.
  explicit LazySyncIdToGuidMapInUpdates(
      const syncer::UpdateResponseDataList* updates)
      : updates_(updates) {
    DCHECK(updates_);
  }

  LazySyncIdToGuidMapInUpdates(const LazySyncIdToGuidMapInUpdates&) = delete;
  LazySyncIdToGuidMapInUpdates& operator=(const LazySyncIdToGuidMapInUpdates&) =
      delete;

  std::string_view GetGuidForSyncId(const std::string& sync_id) {
    InitializeIfNeeded();
    auto it = sync_id_to_guid_map_.find(sync_id);
    if (it == sync_id_to_guid_map_.end()) {
      return std::string_view();
    }
    return it->second;
  }

 private:
  void InitializeIfNeeded() {
    if (initialized_) {
      return;
    }
    initialized_ = true;
    for (const syncer::UpdateResponseData& update : *updates_) {
      std::string_view guid = GetGuidForEntity(update.entity);
      if (!update.entity.id.empty() && !guid.empty()) {
        const bool success =
            sync_id_to_guid_map_.emplace(update.entity.id, guid).second;
        DCHECK(success);
      }
    }
  }

  const raw_ptr<const syncer::UpdateResponseDataList> updates_;
  bool initialized_ = false;
  std::unordered_map<std::string_view, std::string_view> sync_id_to_guid_map_;
};

base::Uuid GetParentGuidForUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker* tracker,
    LazySyncIdToGuidMapInUpdates* sync_id_to_guid_map_in_updates) {
  DCHECK(tracker);
  DCHECK(sync_id_to_guid_map_in_updates);

  if (update.entity.legacy_parent_id.empty()) {
    // Without the |SyncEntity.parent_id| field set, there is no information
    // available to determine the parent and/or its GUID.
    return base::Uuid();
  }

  // If a tracker is available, i.e. initial sync already done, it may know
  // parent's GUID already.
  base::Uuid uuid = TryGetParentGuidFromTracker(tracker, update);
  if (uuid.is_valid()) {
    return uuid;
  }

  // Otherwise, fall back to checking if the parent is included in the full list
  // of updates, represented here by |sync_id_to_guid_map_in_updates|. This
  // codepath is most crucial for initial sync, where |tracker| is empty, but is
  // also useful for non-initial sync, if the same incoming batch creates both
  // parent and child, none of which would be known by |tracker|.
  uuid = base::Uuid::ParseLowercase(
      sync_id_to_guid_map_in_updates->GetGuidForSyncId(
          update.entity.legacy_parent_id));
  if (uuid.is_valid()) {
    return uuid;
  }

  // At this point the parent's GUID couldn't be determined, but actually
  // the |SyncEntity.parent_id| was non-empty. The update will be ignored
  // regardless, but to avoid behavioral differences in UMA metrics
  // Sync.ProblematicServerSideBookmarks[DuringMerge], a fake parent GUID is
  // used here, which is known to never match an existing entity.
  uuid = base::Uuid::ParseLowercase(kInvalidParentGuid);
  DCHECK(uuid.is_valid());
  DCHECK(!tracker->GetEntityForUuid(uuid));
  return uuid;
}

// Same as PopulateParentGuidInSpecifics(), but |tracker| must not be null.
void PopulateParentGuidInSpecificsWithTracker(
    const SyncedBookmarkTracker* tracker,
    syncer::UpdateResponseDataList* updates) {
  DCHECK(tracker);
  DCHECK(updates);

  LazySyncIdToGuidMapInUpdates sync_id_to_guid_map(updates);

  for (syncer::UpdateResponseData& update : *updates) {
    // Only legacy data, without the parent GUID in specifics populated,
    // requires work. This also excludes tombstones and permanent folders.
    if (!NeedsParentGuidInSpecifics(update)) {
      // No work needed.
      continue;
    }

    const base::Uuid uuid =
        GetParentGuidForUpdate(update, tracker, &sync_id_to_guid_map);
    if (uuid.is_valid()) {
      update.entity.specifics.mutable_bookmark()->set_parent_guid(
          uuid.AsLowercaseString());
    }
  }
}

}  // namespace

void PopulateParentGuidInSpecifics(const SyncedBookmarkTracker* tracker,
                                   syncer::UpdateResponseDataList* updates) {
  DCHECK(updates);

  if (tracker) {
    // The code in this file assumes permanent folders are tracked in
    // SyncedBookmarkTracker. Since this is prone to change in the future, the
    // DCHECK below is added to avoid subtle bugs, without relying exclusively
    // on integration tests that exercise legacy data..
    DCHECK(tracker->GetEntityForUuid(
        base::Uuid::ParseLowercase(bookmarks::kBookmarkBarNodeUuid)));
    DCHECK(tracker->GetEntityForUuid(
        base::Uuid::ParseLowercase(bookmarks::kOtherBookmarksNodeUuid)));
    DCHECK(tracker->GetEntityForUuid(
        base::Uuid::ParseLowercase(bookmarks::kMobileBookmarksNodeUuid)));

    PopulateParentGuidInSpecificsWithTracker(tracker, updates);
    return;
  }

  // No tracker provided, so use an empty tracker instead where all lookups will
  // fail.
  std::unique_ptr<SyncedBookmarkTracker> empty_tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  PopulateParentGuidInSpecificsWithTracker(empty_tracker.get(), updates);
}

std::string GetGuidForSyncIdInUpdatesForTesting(  // IN-TEST
    const syncer::UpdateResponseDataList& updates,
    const std::string& sync_id) {
  LazySyncIdToGuidMapInUpdates sync_id_to_guid_map(&updates);
  return std::string(sync_id_to_guid_map.GetGuidForSyncId(sync_id));
}

}  // namespace sync_bookmarks
