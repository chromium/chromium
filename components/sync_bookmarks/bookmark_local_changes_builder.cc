// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_changes_builder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"

namespace sync_bookmarks {

BookmarkLocalChangesBuilder::BookmarkLocalChangesBuilder(
    SyncedBookmarkTracker* const bookmark_tracker,
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_tracker_(bookmark_tracker), bookmark_model_(bookmark_model) {
  DCHECK(bookmark_tracker);
  DCHECK(bookmark_model);
}

syncer::CommitRequestDataList BookmarkLocalChangesBuilder::BuildCommitRequests(
    size_t max_entries) const {
  DCHECK(bookmark_tracker_);
  const std::vector<const SyncedBookmarkTracker::Entity*>
      entities_with_local_changes =
          bookmark_tracker_->GetEntitiesWithLocalChanges(max_entries);
  DCHECK_LE(entities_with_local_changes.size(), max_entries);

  syncer::CommitRequestDataList commit_requests;
  for (const SyncedBookmarkTracker::Entity* entity :
       entities_with_local_changes) {
    DCHECK(entity);
    DCHECK(entity->IsUnsynced());
    const sync_pb::EntityMetadata* metadata = entity->metadata();

    auto data = std::make_unique<syncer::EntityData>();
    data->id = metadata->server_id();
    data->creation_time = syncer::ProtoTimeToTime(metadata->creation_time());
    data->modification_time =
        syncer::ProtoTimeToTime(metadata->modification_time());

    if (bookmark_tracker_->bookmark_client_tags_in_protocol_enabled()) {
      DCHECK(!metadata->client_tag_hash().empty());
      data->client_tag_hash =
          syncer::ClientTagHash::FromHashed(metadata->client_tag_hash());
      DCHECK(metadata->is_deleted() ||
             data->client_tag_hash ==
                 syncer::ClientTagHash::FromUnhashed(
                     syncer::BOOKMARKS,
                     entity->bookmark_node()->guid().AsLowercaseString()));
    }

    if (!metadata->is_deleted()) {
      const bookmarks::BookmarkNode* node = entity->bookmark_node();
      // Skip current entity if its favicon is not loaded yet. It will be
      // committed once the favicon is loaded in
      // BookmarkModelObserverImpl::BookmarkNodeFaviconChanged.
      if (!node->is_folder() && !node->is_favicon_loaded() &&
          !node->is_permanent_node()) {
        // Force the favicon to be loaded. The worker will be nudged for commit
        // in BookmarkModelObserverImpl::BookmarkNodeFaviconChanged() once
        // favicon is loaded.
        bookmark_model_->GetFavicon(node);
        continue;
      }

      DCHECK(node);
      DCHECK_EQ(syncer::ClientTagHash::FromUnhashed(
                    syncer::BOOKMARKS, node->guid().AsLowercaseString()),
                syncer::ClientTagHash::FromHashed(metadata->client_tag_hash()));

      const bookmarks::BookmarkNode* parent = node->parent();
      const SyncedBookmarkTracker::Entity* parent_entity =
          bookmark_tracker_->GetEntityForBookmarkNode(parent);
      DCHECK(parent_entity);
      data->parent_id = parent_entity->metadata()->server_id();
      // TODO(crbug.com/516866): Double check that custom passphrase works well
      // with this implementation, because:
      // 1. syncer::CommitContributionImpl::AdjustCommitProto() clears the
      //    title out.
      // 2. Bookmarks (maybe ancient legacy bookmarks only?) use/used |name| to
      //    encode the title.
      data->is_folder = node->is_folder();
      data->unique_position = metadata->unique_position();
      // Assign specifics only for the non-deletion case. In case of deletion,
      // EntityData should contain empty specifics to indicate deletion.
      data->specifics = CreateSpecificsFromBookmarkNode(
          node, bookmark_model_, /*force_favicon_load=*/true);
      // TODO(crbug.com/1058376): check after finishing if we need to use full
      // title instead of legacy canonicalized one.
      data->name = data->specifics.bookmark().legacy_canonicalized_title();
    }

    auto request = std::make_unique<syncer::CommitRequestData>();
    request->entity = std::move(data);
    request->sequence_number = metadata->sequence_number();
    request->base_version = metadata->server_version();
    // Specifics hash has been computed in the tracker when this entity has been
    // added/updated.
    request->specifics_hash = metadata->specifics_hash();

    bookmark_tracker_->MarkCommitMayHaveStarted(entity);

    commit_requests.push_back(std::move(request));

    // This codepath prevents permanently staying server-side bookmarks without
    // favicons due to an automatically-triggered upload. As far as favicon is
    // loaded the bookmark will be committed again.
    if (!metadata->is_deleted()) {
      const bookmarks::BookmarkNode* node = entity->bookmark_node();
      DCHECK(node);

      if (!node->is_permanent_node() && !node->is_folder() &&
          !node->is_favicon_loaded() &&
          base::FeatureList::IsEnabled(
              switches::kSyncReuploadBookmarkFullTitles)) {
        bookmark_tracker_->IncrementSequenceNumber(entity);
      }
    }
  }
  return commit_requests;
}

}  // namespace sync_bookmarks
