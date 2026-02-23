// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/parent_guid_preprocessing.h"

#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
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

  std::string_view GetGuidForSyncId(std::string_view sync_id) {
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

struct ParentGuidInfo {
  base::Uuid newly_resolved_uuid;
  ParentGuidSource source;
};

std::optional<ParentGuidInfo> GetParentGuidInfo(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker* tracker,
    LazySyncIdToGuidMapInUpdates* sync_id_to_guid_map_in_updates) {
  DCHECK(sync_id_to_guid_map_in_updates);

  // Tombstones and permanent folders don't need a parent GUID.
  if (update.entity.is_deleted() ||
      update.entity.legacy_parent_id == std::string("0") ||
      !update.entity.server_defined_unique_tag.empty()) {
    return std::nullopt;
  }

  // If the parent GUID is already present in specifics, there is no need to
  // resolve it, but we return the source so the caller can log it.
  if (update.entity.specifics.bookmark().has_parent_guid()) {
    return ParentGuidInfo{base::Uuid(), ParentGuidSource::kFoundInSpecifics};
  }

  if (update.entity.legacy_parent_id.empty()) {
    // Without the |SyncEntity.parent_id| field set, there is no information
    // available to determine the parent and/or its GUID.
    return ParentGuidInfo{base::Uuid(), ParentGuidSource::kMissing};
  }

  // If a tracker is available, i.e. initial sync already done, it may know
  // parent's GUID already.
  base::Uuid newly_resolved_uuid;
  if (tracker) {
    newly_resolved_uuid = TryGetParentGuidFromTracker(tracker, update);
    if (newly_resolved_uuid.is_valid()) {
      return ParentGuidInfo{std::move(newly_resolved_uuid),
                            ParentGuidSource::kFallbackFoundInTracker};
    }
  }

  // Otherwise, fall back to checking if the parent is included in the full list
  // of updates, represented here by |sync_id_to_guid_map_in_updates|. This
  // codepath is most crucial for initial sync, where |tracker| is empty, but is
  // also useful for non-initial sync, if the same incoming batch creates both
  // parent and child, none of which would be known by |tracker|.
  newly_resolved_uuid = base::Uuid::ParseLowercase(
      sync_id_to_guid_map_in_updates->GetGuidForSyncId(
          update.entity.legacy_parent_id));
  if (newly_resolved_uuid.is_valid()) {
    return ParentGuidInfo{std::move(newly_resolved_uuid),
                          ParentGuidSource::kFallbackFoundInUpdates};
  }

  // At this point the parent's GUID couldn't be determined, but actually
  // the |SyncEntity.parent_id| was non-empty. The update will be ignored
  // regardless, but to avoid behavioral differences in UMA metrics
  // Sync.ProblematicServerSideBookmarks[DuringMerge], a fake parent GUID is
  // used here, which is known to never match an existing entity.
  newly_resolved_uuid = base::Uuid::ParseLowercase(kInvalidParentGuid);
  DCHECK(newly_resolved_uuid.is_valid());
  DCHECK(!tracker || !tracker->GetEntityForUuid(newly_resolved_uuid));
  return ParentGuidInfo{std::move(newly_resolved_uuid),
                        ParentGuidSource::kFallbackUnresolvable};
}

void LogParentGuidSource(ParentGuidSource source) {
  base::UmaHistogramEnumeration("Sync.BookmarkParentGuidSource", source);
  base::UmaHistogramBoolean("Sync.BookmarkParentGuidFromSpecifics",
                            source == ParentGuidSource::kFoundInSpecifics);
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
  }

  LazySyncIdToGuidMapInUpdates sync_id_to_guid_map(updates);

  for (syncer::UpdateResponseData& update : *updates) {
    std::optional<ParentGuidInfo> info =
        GetParentGuidInfo(update, tracker, &sync_id_to_guid_map);
    if (!info) {
      continue;
    }

    LogParentGuidSource(info->source);
    if (info->newly_resolved_uuid.is_valid()) {
      update.entity.specifics.mutable_bookmark()->set_parent_guid(
          info->newly_resolved_uuid.AsLowercaseString());
    }
  }
}

std::string GetGuidForSyncIdInUpdatesForTesting(  // IN-TEST
    const syncer::UpdateResponseDataList& updates,
    const std::string& sync_id) {
  LazySyncIdToGuidMapInUpdates sync_id_to_guid_map(&updates);
  return std::string(sync_id_to_guid_map.GetGuidForSyncId(sync_id));
}

}  // namespace sync_bookmarks
