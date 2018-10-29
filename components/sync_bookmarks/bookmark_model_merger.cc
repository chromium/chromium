// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"

using syncer::EntityData;
using syncer::UpdateResponseData;
using syncer::UpdateResponseDataList;

namespace sync_bookmarks {

namespace {

static const size_t kInvalidIndex = -1;

// The sync protocol identifies top-level entities by means of well-known tags,
// (aka server defined tags) which should not be confused with titles or client
// tags that aren't supported by bookmarks (at the time of writing). Each tag
// corresponds to a singleton instance of a particular top-level node in a
// user's share; the tags are consistent across users. The tags allow us to
// locate the specific folders whose contents we care about synchronizing,
// without having to do a lookup by name or path.  The tags should not be made
// user-visible. For example, the tag "bookmark_bar" represents the permanent
// node for bookmarks bar in Chrome. The tag "other_bookmarks" represents the
// permanent folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing, the sync
// server) to create these tagged nodes when initializing sync for the first
// time for a user.  Thus, once the backend finishes initializing, the
// ProfileSyncService can rely on the presence of tagged nodes.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Heuristic to consider two nodes (local and remote) a match for the purpose of
// merging. Two folders match if they have the same title, two bookmarks match
// if they have the same title and url. A folder and a bookmark never match.
bool NodesMatch(const bookmarks::BookmarkNode* local_node,
                const EntityData& remote_node) {
  if (local_node->is_folder() != remote_node.is_folder) {
    return false;
  }
  const sync_pb::BookmarkSpecifics& specifics =
      remote_node.specifics.bookmark();
  if (local_node->GetTitle() != base::UTF8ToUTF16(specifics.title())) {
    return false;
  }
  if (remote_node.is_folder) {
    return true;
  }
  return local_node->url() == GURL(specifics.url());
}

// Tries to find a child local node under |local_parent| that matches
// |remote_node| and returns the index of that node. Matching is decided using
// NodesMatch(). It starts searching in the children list starting from position
// |search_starting_child_index|. In case of no match is found, it returns
// |kInvalidIndex|.
size_t FindMatchingChildFor(const UpdateResponseData* remote_node,
                            const bookmarks::BookmarkNode* local_parent,
                            size_t search_starting_child_index) {
  const EntityData& remote_node_update_entity = remote_node->entity.value();
  for (int i = search_starting_child_index; i < local_parent->child_count();
       ++i) {
    const bookmarks::BookmarkNode* local_child = local_parent->GetChild(i);
    if (NodesMatch(local_child, remote_node_update_entity)) {
      return i;
    }
  }
  return kInvalidIndex;
}

bool UniquePositionLessThan(const UpdateResponseData* a,
                            const UpdateResponseData* b) {
  const syncer::UniquePosition a_pos =
      syncer::UniquePosition::FromProto(a->entity.value().unique_position);
  const syncer::UniquePosition b_pos =
      syncer::UniquePosition::FromProto(b->entity.value().unique_position);
  return a_pos.LessThan(b_pos);
}

// Builds a map from node update to a vector of its children updates. The vector
// is sorted according to the unique position information in each update. A
// entry is only added for an update if it has children updates with the
// exception of permanent folders. Updates of permanent folders always exist in
// the returned folder. If they don't have children, each is asoociated with an
// empty vector of updates. Returned map contains pointers to the elements in
// |updates|.
std::unordered_map<const UpdateResponseData*,
                   std::vector<const UpdateResponseData*>>
BuildUpdatesTreeWithoutTombstonesWithSortedChildren(
    const UpdateResponseDataList* updates) {
  std::unordered_map<const UpdateResponseData*,
                     std::vector<const UpdateResponseData*>>
      updates_tree;
  std::unordered_map<base::StringPiece, const UpdateResponseData*,
                     base::StringPieceHash>
      id_to_updates;
  // Tombstones carry only the sync id and cannot be merged with the local
  // model. Hence, we ignore tombstones.
  for (const UpdateResponseData& update : *updates) {
    const EntityData& update_entity = update.entity.value();
    if (update_entity.is_deleted()) {
      continue;
    }
    id_to_updates[update_entity.id] = &update;
  }

  for (const UpdateResponseData& update : *updates) {
    const EntityData& update_entity = update.entity.value();
    if (update_entity.is_deleted()) {
      continue;
    }
    // No need to associate permanent nodes with their parent (the root node).
    // We start merging from the permanent nodes.
    if (!update_entity.server_defined_unique_tag.empty()) {
      continue;
    }
    if (!syncer::UniquePosition::FromProto(update_entity.unique_position)
             .IsValid()) {
      // Ignore updates with invalid positions.
      DLOG(ERROR) << "Remote update with invalid position: "
                  << update_entity.specifics.bookmark().title();
      continue;
    }
    if (!IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                  update_entity.is_folder)) {
      // Ignore updates with invalid specifics.
      DLOG(ERROR) << "Remote update with invalid specifics";
      continue;
    }
    const UpdateResponseData* parent_update =
        id_to_updates[update_entity.parent_id];
    updates_tree[parent_update].push_back(&update);
  }

  // Sort all child updates.
  for (std::pair<const UpdateResponseData* const,
                 std::vector<const UpdateResponseData*>>& pair : updates_tree) {
    std::sort(pair.second.begin(), pair.second.end(), UniquePositionLessThan);
  }
  return updates_tree;
}

}  // namespace

BookmarkModelMerger::BookmarkModelMerger(
    const UpdateResponseDataList* updates,
    bookmarks::BookmarkModel* bookmark_model,
    favicon::FaviconService* favicon_service,
    SyncedBookmarkTracker* bookmark_tracker)
    : updates_(updates),
      bookmark_model_(bookmark_model),
      favicon_service_(favicon_service),
      bookmark_tracker_(bookmark_tracker),
      updates_tree_(
          BuildUpdatesTreeWithoutTombstonesWithSortedChildren(updates_)) {
  DCHECK(bookmark_tracker_->IsEmpty());
  DCHECK(favicon_service);
}

BookmarkModelMerger::~BookmarkModelMerger() {}

void BookmarkModelMerger::Merge() {
  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each sync node for the current sync parent node, find the best
  //   matching bookmark node under the corresponding bookmark parent node.
  //   If no matching node is found, create a new bookmark node in the same
  //   position as the corresponding sync node.
  //   If a matching node is found, update the properties of it from the
  //   corresponding sync node.
  // * When all children sync nodes are done, add the extra children bookmark
  //   nodes to the sync parent node.
  //
  // The best match algorithm uses folder title or bookmark title/url to
  // perform the primary match. If there are multiple match candidates it
  // selects the first one.
  // Associate permanent folders.
  for (const UpdateResponseData& update : *updates_) {
    const EntityData& update_entity = update.entity.value();
    const bookmarks::BookmarkNode* permanent_folder =
        GetPermanentFolder(update_entity);
    if (!permanent_folder) {
      continue;
    }
    MergeSubtree(permanent_folder, &update);
  }
  // TODO(crbug.com/516866): Check that both models match now.

  // TODO(crbug.com/516866): What if no permanent nodes updates are sent from
  // the server, check if this is a legit scenario.
}

void BookmarkModelMerger::MergeSubtree(
    const bookmarks::BookmarkNode* local_node,
    const UpdateResponseData* remote_update) {
  const EntityData& remote_update_entity = remote_update->entity.value();
  bookmark_tracker_->Add(
      remote_update_entity.id, local_node, remote_update->response_version,
      remote_update_entity.creation_time, remote_update_entity.unique_position,
      remote_update_entity.specifics);
  // If there are remote child updates, try to match them.
  if (updates_tree_.count(remote_update) != 0) {
    const std::vector<const UpdateResponseData*> remote_children =
        updates_tree_.at(remote_update);
    for (size_t remote_index = 0; remote_index < remote_children.size();
         ++remote_index) {
      // All local nodes up to |remote_index-1| have processed already. Look for
      // a matching local node starting with the local node at postion
      // |remote_index|.
      const size_t local_index = FindMatchingChildFor(
          /*remote_node=*/remote_children[remote_index],
          /*local_parent=*/local_node,
          /*search_starting_index=*/remote_index);
      if (local_index == kInvalidIndex) {
        // If no match found, create a corresponding local node.
        ProcessRemoteCreation(remote_children[remote_index], local_node,
                              remote_index);
        continue;
      }
      const bookmarks::BookmarkNode* local_child =
          local_node->GetChild(local_index);
      // If local node isn't in the correct position, move it.
      if (remote_index != local_index) {
        bookmark_model_->Move(local_child, local_node, remote_index);
      }
      // Since nodes are matching, their subtrees should be merged as well.
      MergeSubtree(local_child, remote_children[remote_index]);
    }
  }
  // At this point all the children nodes of the parent sync node have
  // corresponding children in the parent bookmark node and they are all in the
  // right positions: from 0 to |updates_tree_.at(remote_update).size() - 1|. So
  // the children starting from index |updates_tree_.at(remote_update).size()|
  // in the parent bookmark node are the ones that are not present in the parent
  // sync node and tracked yet. So create all of the remaining local nodes.
  const int index_of_new_local_nodes =
      updates_tree_.count(remote_update) > 0
          ? updates_tree_.at(remote_update).size()
          : 0;
  for (int i = index_of_new_local_nodes; i < local_node->child_count(); ++i) {
    ProcessLocalCreation(local_node, i);
  }
}

void BookmarkModelMerger::ProcessRemoteCreation(
    const UpdateResponseData* remote_update,
    const bookmarks::BookmarkNode* local_parent,
    int index) {
  const EntityData& remote_update_entity = remote_update->entity.value();
  const bookmarks::BookmarkNode* bookmark_node =
      CreateBookmarkNodeFromSpecifics(
          remote_update_entity.specifics.bookmark(), local_parent, index,
          remote_update_entity.is_folder, bookmark_model_, favicon_service_);
  if (!bookmark_node) {
    // We ignore bookmarks we can't add.
    DLOG(ERROR) << "Failed to create bookmark node with title "
                << remote_update_entity.specifics.bookmark().title()
                << " and url "
                << remote_update_entity.specifics.bookmark().url();
    return;
  }
  bookmark_tracker_->Add(
      remote_update_entity.id, bookmark_node, remote_update->response_version,
      remote_update_entity.creation_time, remote_update_entity.unique_position,
      remote_update_entity.specifics);
  // If no child update, we are done.
  if (updates_tree_.count(remote_update) == 0) {
    return;
  }
  // Recusively, create local node for all child remote nodes.
  int i = 0;
  for (const UpdateResponseData* remote_child_update :
       updates_tree_.at(remote_update)) {
    ProcessRemoteCreation(remote_child_update, bookmark_node, i++);
  }
}

void BookmarkModelMerger::ProcessLocalCreation(
    const bookmarks::BookmarkNode* parent,
    int index) {
  DCHECK_GT(index, -1);
  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForBookmarkNode(parent);
  // Since we are merging top down, parent entity must be tracked.
  DCHECK(parent_entity);

  // Similar to the diectory implementation here:
  // https://cs.chromium.org/chromium/src/components/sync/syncable/mutable_entry.cc?l=237&gsn=CreateEntryKernel
  // Assign a temp server id for the entity. Will be overriden by the actual
  // server id upon receiving commit response.
  const std::string sync_id = base::GenerateGUID();
  const int64_t server_version = syncer::kUncommittedVersion;
  const base::Time creation_time = base::Time::Now();
  const std::string& suffix = syncer::GenerateSyncableBookmarkHash(
      bookmark_tracker_->model_type_state().cache_guid(), sync_id);
  syncer::UniquePosition pos;
  // Locally created nodes aren't tracked and hence don't have a unique position
  // yet so we need to produce new ones.
  if (index == 0) {
    pos = syncer::UniquePosition::InitialPosition(suffix);
  } else {
    const SyncedBookmarkTracker::Entity* predecessor_entity =
        bookmark_tracker_->GetEntityForBookmarkNode(
            parent->GetChild(index - 1));
    pos = syncer::UniquePosition::After(
        syncer::UniquePosition::FromProto(
            predecessor_entity->metadata()->unique_position()),
        suffix);
  }

  const bookmarks::BookmarkNode* node = parent->GetChild(index);
  const sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, /*force_favicon_load=*/true);
  bookmark_tracker_->Add(sync_id, node, server_version, creation_time,
                         pos.ToProto(), specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(sync_id);
  for (int i = 0; i < node->child_count(); ++i) {
    // If a local node hasn't matched with any remote entity, its descendants
    // will neither.
    ProcessLocalCreation(node, i);
  }
}

const bookmarks::BookmarkNode* BookmarkModelMerger::GetPermanentFolder(
    const EntityData& update_entity) const {
  const bookmarks::BookmarkNode* permanent_folder = nullptr;
  if (update_entity.server_defined_unique_tag == kBookmarkBarTag) {
    permanent_folder = bookmark_model_->bookmark_bar_node();
  } else if (update_entity.server_defined_unique_tag == kOtherBookmarksTag) {
    permanent_folder = bookmark_model_->other_node();
  } else if (update_entity.server_defined_unique_tag == kMobileBookmarksTag) {
    permanent_folder = bookmark_model_->mobile_node();
  }
  return permanent_folder;
}

}  // namespace sync_bookmarks
