// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_changes_builder.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"

namespace sync_bookmarks {

BookmarkLocalChangesBuilder::BookmarkLocalChangesBuilder(
    SyncedBookmarkTracker* const bookmark_tracker,
    BookmarkModelView* bookmark_model)
    : bookmark_tracker_(bookmark_tracker), bookmark_model_(bookmark_model) {
  DCHECK(bookmark_tracker);
  DCHECK(bookmark_model);
}

syncer::CommitRequestDataList BookmarkLocalChangesBuilder::BuildCommitRequests(
    size_t max_entries) const {
  DCHECK(bookmark_tracker_);

  const std::vector<const SyncedBookmarkTrackerEntity*>
      entities_with_local_changes =
          bookmark_tracker_->GetEntitiesWithLocalChanges();

  syncer::CommitRequestDataList commit_requests;
  for (const SyncedBookmarkTrackerEntity* entity :
       entities_with_local_changes) {
    if (commit_requests.size() >= max_entries) {
      break;
    }

    DCHECK(entity);
    DCHECK(entity->IsUnsynced());
    const sync_pb::EntityMetadata& metadata = entity->metadata();

    auto data = std::make_unique<syncer::EntityData>();
    data->id = metadata.server_id();
    data->creation_time = syncer::ProtoTimeToTime(metadata.creation_time());
    data->modification_time =
        syncer::ProtoTimeToTime(metadata.modification_time());

    DCHECK(!metadata.client_tag_hash().empty());
    data->client_tag_hash =
        syncer::ClientTagHash::FromHashed(metadata.client_tag_hash());
    DCHECK(metadata.is_deleted() ||
           data->client_tag_hash ==
               syncer::ClientTagHash::FromUnhashed(
                   syncer::BOOKMARKS,
                   entity->bookmark_node()->uuid().AsLowercaseString()));

    if (metadata.is_deleted()) {
      // Absence of deletion origin is primarily needed for pre-existing
      // tombstones in storage before this field was introduced. Nevertheless,
      // it seems best to treat it as optional here, in case some codepaths
      // don't provide it in the future.
      if (metadata.has_deletion_origin()) {
        data->deletion_origin = metadata.deletion_origin();
      }
    } else {
      const bookmarks::BookmarkNode* node = entity->bookmark_node();
      DCHECK(!node->is_permanent_node());

      // Skip current entity if its favicon is not loaded yet. It will be
      // committed once the favicon is loaded in
      // BookmarkModelObserverImpl::BookmarkNodeFaviconChanged.
      if (!node->is_folder() && !node->is_favicon_loaded()) {
        // Force the favicon to be loaded. The worker will be nudged for commit
        // in BookmarkModelObserverImpl::BookmarkNodeFaviconChanged() once
        // favicon is loaded.
        bookmark_model_->GetFavicon(node);
        continue;
      }

      DCHECK(node);
      DCHECK_EQ(syncer::ClientTagHash::FromUnhashed(
                    syncer::BOOKMARKS, node->uuid().AsLowercaseString()),
                syncer::ClientTagHash::FromHashed(metadata.client_tag_hash()));

      const bookmarks::BookmarkNode* parent = node->parent();
      const SyncedBookmarkTrackerEntity* parent_entity =
          bookmark_tracker_->GetEntityForBookmarkNode(parent);
      DCHECK(parent_entity);
      data->legacy_parent_id = parent_entity->metadata().server_id();
      // Assign specifics only for the non-deletion case. In case of deletion,
      // EntityData should contain empty specifics to indicate deletion.
      data->specifics = CreateSpecificsFromBookmarkNode(
          node, bookmark_model_, metadata.unique_position(),
          /*force_favicon_load=*/true);
      // TODO(crbug.com/40677937): check after finishing if we need to use full
      // title instead of legacy canonicalized one.
      data->name = data->specifics.bookmark().legacy_canonicalized_title();
    }

    auto request = std::make_unique<syncer::CommitRequestData>();
    request->entity = std::move(data);
    request->sequence_number = metadata.sequence_number();
    request->base_version = metadata.server_version();
    // Specifics hash has been computed in the tracker when this entity has been
    // added/updated.
    request->specifics_hash = metadata.specifics_hash();

    if (!metadata.is_deleted()) {
      const bookmarks::BookmarkNode* node = entity->bookmark_node();
      CHECK(node);
      request->deprecated_bookmark_folder = node->is_folder();
      request->deprecated_bookmark_unique_position =
          syncer::UniquePosition::FromProto(metadata.unique_position());
    }

    bookmark_tracker_->MarkCommitMayHaveStarted(entity);

    commit_requests.push_back(std::move(request));
  }
  return commit_requests;
}

}  // namespace sync_bookmarks
