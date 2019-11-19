// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "ui/base/models/tree_node_iterator.h"

using syncer::EntityData;
using syncer::UpdateResponseData;
using syncer::UpdateResponseDataList;

namespace sync_bookmarks {

namespace {

static const size_t kInvalidIndex = -1;

// Maximum number of bytes to allow in a title (must match sync's internal
// limits; see write_node.cc).
const int kTitleLimitBytes = 255;

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

// Canonicalize |title| similar to legacy client's implementation by truncating
// up to |kTitleLimitBytes| and the appending ' ' in some cases.
std::string CanonicalizeTitle(const std::string& title) {
  std::string canonical_title;
  syncer::SyncAPINameToServerName(title, &canonical_title);
  base::TruncateUTF8ToByteSize(canonical_title, kTitleLimitBytes,
                               &canonical_title);
  return canonical_title;
}

// Heuristic to consider two nodes (local and remote) a match by semantics for
// the purpose of merging. Two folders match by semantics if they have the same
// title, two bookmarks match by semantics if they have the same title and url.
// A folder and a bookmark never match.
bool NodeSemanticsMatch(const bookmarks::BookmarkNode* local_node,
                        const EntityData& remote_node) {
  if (local_node->is_folder() != remote_node.is_folder) {
    return false;
  }
  const sync_pb::BookmarkSpecifics& specifics =
      remote_node.specifics.bookmark();
  const std::string local_title = base::UTF16ToUTF8(local_node->GetTitle());
  const std::string remote_title = specifics.title();
  // Titles match if they are identical or the remote one is the canonical form
  // of the local one. The latter is the case when a legacy client has
  // canonicalized the same local title before committing it. Modern clients
  // don't canonicalize titles anymore.
  if (local_title != remote_title &&
      CanonicalizeTitle(local_title) != remote_title) {
    return false;
  }
  if (remote_node.is_folder) {
    return true;
  }
  return local_node->url() == GURL(specifics.url());
}

bool UniquePositionLessThan(const UpdateResponseData* a,
                            const UpdateResponseData* b) {
  const syncer::UniquePosition a_pos =
      syncer::UniquePosition::FromProto(a->entity->unique_position);
  const syncer::UniquePosition b_pos =
      syncer::UniquePosition::FromProto(b->entity->unique_position);
  return a_pos.LessThan(b_pos);
}

// Builds a map from node update to a vector of its children updates. The vector
// is sorted according to the unique position information in each update. An
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
  for (const std::unique_ptr<syncer::UpdateResponseData>& update : *updates) {
    DCHECK(update);
    const EntityData& update_entity = *update->entity;
    if (update_entity.is_deleted()) {
      continue;
    }
    id_to_updates[update_entity.id] = update.get();
  }

  for (const std::unique_ptr<UpdateResponseData>& update : *updates) {
    DCHECK(update);
    const EntityData& update_entity = *update->entity;
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
    updates_tree[parent_update].push_back(update.get());
  }

  // Sort all child updates.
  for (auto& parent_update_and_chilren : updates_tree) {
    std::sort(parent_update_and_chilren.second.begin(),
              parent_update_and_chilren.second.end(), UniquePositionLessThan);
  }
  return updates_tree;
}

// Populates a map from GUID to corresponding remote update, used in the merge
// process to determine GUID-based node matches.
std::unordered_map<std::string, const syncer::UpdateResponseData*>
BuildGUIDToRemoteUpdateMap(const UpdateResponseDataList* updates) {
  // TODO(crbug.com/978430): Handle potential duplicate GUIDs within remote
  // updates.
  // TODO(crbug.com/978430): Verify the validity of the specifics.
  std::unordered_map<std::string, const syncer::UpdateResponseData*>
      guid_to_remote_update_map;
  if (!base::FeatureList::IsEnabled(switches::kMergeBookmarksUsingGUIDs)) {
    return guid_to_remote_update_map;
  }
  for (const std::unique_ptr<UpdateResponseData>& update : *updates) {
    EntityData* update_entity = update.get()->entity.get();
    DCHECK(update_entity->specifics.bookmark().guid().empty() ||
           base::IsValidGUID(update_entity->specifics.bookmark().guid()));
    if (!update_entity->specifics.bookmark().guid().empty()) {
      guid_to_remote_update_map.emplace(
          update_entity->specifics.bookmark().guid(), update.get());
    }
  }
  return guid_to_remote_update_map;
}

// Populates a map from GUID to corresponding local node, used in the merge
// process to determine GUID-based node matches.
std::unordered_map<std::string, const bookmarks::BookmarkNode*>
BuildGUIDToLocalNodeMap(
    bookmarks::BookmarkModel* bookmark_model,
    std::unordered_map<std::string, const syncer::UpdateResponseData*>
        guid_to_remote_update_map) {
  std::unordered_map<std::string, const bookmarks::BookmarkNode*>
      guid_to_local_node_map;
  if (!base::FeatureList::IsEnabled(switches::kMergeBookmarksUsingGUIDs)) {
    return guid_to_local_node_map;
  }
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmark_model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    DCHECK(base::IsValidGUID(node->guid()));
    if (node->is_permanent_node()) {
      continue;
    }
    // If local node and its remote node match are conflicting in node type or
    // URL, replace local GUID with a random GUID.
    if (guid_to_remote_update_map.count(node->guid()) != 0) {
      const syncer::EntityData* remote_node =
          guid_to_remote_update_map.find(node->guid())->second->entity.get();
      if (node->is_folder() != remote_node->is_folder ||
          (node->is_url() &&
           node->url() != remote_node->specifics.bookmark().url())) {
        node =
            ReplaceBookmarkNodeGUID(node, base::GenerateGUID(), bookmark_model);
      }
    }
    guid_to_local_node_map.emplace(node->guid(), node);
  }
  return guid_to_local_node_map;
}

}  // namespace

BookmarkModelMerger::BookmarkModelMerger(
    UpdateResponseDataList updates,
    bookmarks::BookmarkModel* bookmark_model,
    favicon::FaviconService* favicon_service,
    SyncedBookmarkTracker* bookmark_tracker)
    : original_updates_(std::move(updates)),
      bookmark_model_(bookmark_model),
      favicon_service_(favicon_service),
      bookmark_tracker_(bookmark_tracker),
      updates_tree_(BuildUpdatesTreeWithoutTombstonesWithSortedChildren(
          &original_updates_)),
      guid_to_remote_update_map_(
          BuildGUIDToRemoteUpdateMap(&original_updates_)),
      guid_to_local_node_map_(
          BuildGUIDToLocalNodeMap(bookmark_model_,
                                  guid_to_remote_update_map_)) {
  DCHECK(bookmark_tracker_->IsEmpty());
  DCHECK(favicon_service);
}

BookmarkModelMerger::~BookmarkModelMerger() {}

void BookmarkModelMerger::Merge() {
  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each remote node for the current remote (sync) parent node, either
  //   find a local node with equal GUID anywhere throughout the tree or find
  //   the best matching bookmark node under the corresponding local bookmark
  //   parent node using semantics. If the found node has the same GUID as a
  //   different remote bookmark, we do not consider it a semantics match, as
  //   GUID matching takes precedence. If no matching node is found, create a
  //   new bookmark node in the same position as the corresponding remote node.
  //   If a matching node is found, update the properties of it from the
  //   corresponding remote node.
  // * When all children remote nodes are done, add the extra children bookmark
  //   nodes to the remote (sync) parent node, unless they will be later matched
  //   by GUID.
  //
  // The semantics best match algorithm uses folder title or bookmark title/url
  // to perform the primary match. If there are multiple match candidates it
  // selects the first one.
  // Associate permanent folders.
  for (const std::unique_ptr<UpdateResponseData>& update : original_updates_) {
    DCHECK(update);
    const EntityData& update_entity = *update->entity;
    const bookmarks::BookmarkNode* permanent_folder =
        GetPermanentFolder(update_entity);
    if (!permanent_folder) {
      continue;
    }
    MergeSubtree(permanent_folder, update.get());
  }
  // TODO(crbug.com/516866): Check that both models match now.

  // TODO(crbug.com/516866): What if no permanent nodes updates are sent from
  // the server, check if this is a legit scenario.
}

void BookmarkModelMerger::MergeSubtree(
    const bookmarks::BookmarkNode* local_subtree_root,
    const UpdateResponseData* remote_update) {
  const EntityData& remote_update_entity = *remote_update->entity;
  bookmark_tracker_->Add(
      remote_update_entity.id, local_subtree_root,
      remote_update->response_version, remote_update_entity.creation_time,
      remote_update_entity.unique_position, remote_update_entity.specifics);
  // If there are remote child updates, try to match them.
  if (updates_tree_.count(remote_update) != 0) {
    const std::vector<const UpdateResponseData*> remote_children =
        updates_tree_.at(remote_update);
    for (size_t remote_index = 0; remote_index < remote_children.size();
         ++remote_index) {
      const syncer::UpdateResponseData* remote_child =
          remote_children[remote_index];
      const bookmarks::BookmarkNode* matching_local_node =
          FindMatchingLocalNode(remote_child, local_subtree_root, remote_index);
      // If no match found, create a corresponding local node.
      if (!matching_local_node) {
        ProcessRemoteCreation(remote_child, local_subtree_root, remote_index);
        continue;
      }
      DCHECK(!local_subtree_root->HasAncestor(matching_local_node));
      // Move if required, no-op otherwise.
      bookmark_model_->Move(matching_local_node, local_subtree_root,
                            remote_index);
      // Since nodes are matching, their subtrees should be merged as well.
      matching_local_node = UpdateBookmarkNodeFromSpecificsIncludingGUID(
          matching_local_node, remote_child);
      MergeSubtree(matching_local_node, remote_child);
    }
  }
  // At this point all the children nodes of the parent sync node have
  // corresponding children in the parent bookmark node and they are all in the
  // right positions: from 0 to updates_tree_.at(remote_update).size() - 1. So
  // the children starting from index updates_tree_.at(remote_update).size() in
  // the parent bookmark node are the ones that are not present in the parent
  // sync node and tracked yet. So create all of the remaining local nodes.
  const size_t index_of_new_local_nodes =
      updates_tree_.count(remote_update) > 0
          ? updates_tree_.at(remote_update).size()
          : 0;
  for (size_t i = index_of_new_local_nodes;
       i < local_subtree_root->children().size(); ++i) {
    // TODO(crbug.com/978430): Handle potential orphan nodes that could impede
    // local GUID matches from being processed as local creations.
    // If local node has been or will be matched by GUID, skip it.
    if (FindMatchingRemoteUpdateByGUID(
            local_subtree_root->children()[i].get())) {
      continue;
    }
    ProcessLocalCreation(local_subtree_root, i);
  }
}

const bookmarks::BookmarkNode* BookmarkModelMerger::FindMatchingLocalNode(
    const syncer::UpdateResponseData* remote_child,
    const bookmarks::BookmarkNode* local_parent,
    size_t local_child_start_index) const {
  // Try to match child by GUID. If we can't, try to match child by semantics.
  const bookmarks::BookmarkNode* matching_local_node =
      FindMatchingLocalNodeByGUID(*remote_child);
  if (!matching_local_node) {
    // All local nodes up to |remote_index-1| have processed already. Look for a
    // matching local node starting with the local node at position
    // |local_child_start_index|. FindMatchingChildBySemanticsStartingAt()
    // returns kInvalidIndex in the case where no semantics match was found or
    // the semantics match found is GUID-matchable to a different node.
    const size_t local_index = FindMatchingChildBySemanticsStartingAt(
        /*remote_node=*/remote_child,
        /*local_parent=*/local_parent,
        /*starting_child_index=*/local_child_start_index);
    if (local_index == kInvalidIndex) {
      // If no match found, return.
      return nullptr;
    }
    matching_local_node = local_parent->children()[local_index].get();
  }
  return matching_local_node;
}

const bookmarks::BookmarkNode*
BookmarkModelMerger::UpdateBookmarkNodeFromSpecificsIncludingGUID(
    const bookmarks::BookmarkNode* local_node,
    const UpdateResponseData* remote_update) {
  DCHECK(!local_node->is_permanent_node());
  // Ensure bookmarks have the same URL, otherwise they would not have been
  // matched.
  DCHECK(local_node->is_folder() ||
         local_node->url() ==
             GURL(remote_update->entity->specifics.bookmark().url()));
  const EntityData& remote_update_entity = *remote_update->entity;
  const sync_pb::BookmarkSpecifics specifics =
      remote_update_entity.specifics.bookmark();

  // If the nodes were matched by GUID, we update the BookmarkNode semantics
  // accordingly.
  if (local_node->guid() == specifics.guid()) {
    UpdateBookmarkNodeFromSpecifics(specifics, local_node, bookmark_model_,
                                    favicon_service_);
  }

  // If the nodes were matched by semantics, the local GUID is replaced by its
  // remote counterpart, unless it is empty, in which case we keep the local
  // GUID unchanged.
  if (specifics.guid().empty() || FindMatchingLocalNodeByGUID(*remote_update)) {
    return local_node;
  }
  DCHECK(base::IsValidGUID(specifics.guid()));
  // We do not update the GUID maps upon node replacement as per the comment
  // in bookmark_model_merger.h.
  return ReplaceBookmarkNodeGUID(local_node, specifics.guid(), bookmark_model_);
}

void BookmarkModelMerger::ProcessRemoteCreation(
    const UpdateResponseData* remote_update,
    const bookmarks::BookmarkNode* local_parent,
    size_t index) {
  DCHECK(!FindMatchingLocalNodeByGUID(*remote_update));
  const EntityData& remote_update_entity = *remote_update->entity;

  // If specifics do not have a valid GUID, create a new one. Legacy clients do
  // not populate GUID field and if the originator_client_item_id is not of
  // valid GUID format to replace it, the field is left blank.
  if (!base::IsValidGUID(remote_update_entity.specifics.bookmark().guid())) {
    remote_update->entity->specifics.mutable_bookmark()->set_guid(
        base::GenerateGUID());
  }

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
  // Recursively, match by GUID or, if not possible, create local node for all
  // child remote nodes.
  int i = 0;
  for (const UpdateResponseData* remote_child_update :
       updates_tree_.at(remote_update)) {
    const bookmarks::BookmarkNode* local_child =
        FindMatchingLocalNodeByGUID(*remote_child_update);
    if (!local_child) {
      ProcessRemoteCreation(remote_child_update, bookmark_node, i++);
      continue;
    }
    bookmark_model_->Move(local_child, bookmark_node, i++);
    local_child = UpdateBookmarkNodeFromSpecificsIncludingGUID(
        local_child, remote_child_update);
    MergeSubtree(local_child, remote_child_update);
  }
}

void BookmarkModelMerger::ProcessLocalCreation(
    const bookmarks::BookmarkNode* parent,
    size_t index) {
  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForBookmarkNode(parent);
  // Since we are merging top down, parent entity must be tracked.
  DCHECK(parent_entity);

  // Similar to the directory implementation here:
  // https://cs.chromium.org/chromium/src/components/sync/syncable/mutable_entry.cc?l=237&gsn=CreateEntryKernel
  // Assign a temp server id for the entity. Will be overridden by the actual
  // server id upon receiving commit response.
  const bookmarks::BookmarkNode* node = parent->children()[index].get();
  DCHECK(!FindMatchingRemoteUpdateByGUID(node));
  DCHECK(base::IsValidGUID(node->guid()));
  const std::string sync_id =
      base::FeatureList::IsEnabled(switches::kMergeBookmarksUsingGUIDs)
          ? node->guid()
          : base::GenerateGUID();
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
            parent->children()[index - 1].get());
    pos = syncer::UniquePosition::After(
        syncer::UniquePosition::FromProto(
            predecessor_entity->metadata()->unique_position()),
        suffix);
  }
  const sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, /*force_favicon_load=*/true);
  bookmark_tracker_->Add(sync_id, node, server_version, creation_time,
                         pos.ToProto(), specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(sync_id);
  for (size_t i = 0; i < node->children().size(); ++i) {
    // If a local node hasn't matched with any remote entity, its descendants
    // will neither, unless they have been or will be matched by GUID, in which
    // case we skip them for now.
    if (FindMatchingRemoteUpdateByGUID(node->children()[i].get())) {
      continue;
    }
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

size_t BookmarkModelMerger::FindMatchingChildBySemanticsStartingAt(
    const UpdateResponseData* remote_node,
    const bookmarks::BookmarkNode* local_parent,
    size_t starting_child_index) const {
  const auto& children = local_parent->children();
  DCHECK_LE(starting_child_index, children.size());
  const EntityData& remote_entity = *remote_node->entity;
  const auto it =
      std::find_if(children.cbegin() + starting_child_index, children.cend(),
                   [this, &remote_entity](const auto& child) {
                     return NodeSemanticsMatch(child.get(), remote_entity) &&
                            !FindMatchingRemoteUpdateByGUID(child.get());
                   });
  return (it == children.cend()) ? kInvalidIndex : (it - children.cbegin());
}

const syncer::UpdateResponseData*
BookmarkModelMerger::FindMatchingRemoteUpdateByGUID(
    const bookmarks::BookmarkNode* local_node) const {
  DCHECK(local_node);
  // Ensure matching nodes are of the same type and have the same URL,
  // guaranteed by BuildGUIDToLocalNodeMap().
  const auto it = guid_to_remote_update_map_.find(local_node->guid());
  if (it == guid_to_remote_update_map_.end()) {
    return nullptr;
  }
  const syncer::UpdateResponseData* remote_update = it->second;
  const syncer::EntityData* remote_node = remote_update->entity.get();
  DCHECK_EQ(local_node->is_folder(), remote_node->is_folder);
  DCHECK_EQ(local_node->url(), remote_node->specifics.bookmark().url());
  return remote_update;
}

const bookmarks::BookmarkNode* BookmarkModelMerger::FindMatchingLocalNodeByGUID(
    const syncer::UpdateResponseData& remote_update) const {
  DCHECK(&remote_update);
  const syncer::EntityData* remote_node = remote_update.entity.get();
  const auto it =
      guid_to_local_node_map_.find(remote_node->specifics.bookmark().guid());
  if (it == guid_to_local_node_map_.end()) {
    return nullptr;
  }
  DCHECK(!remote_node->specifics.bookmark().guid().empty());
  const bookmarks::BookmarkNode* local_node = it->second;
  // Ensure matching nodes are of the same type and have the same URL,
  // guaranteed by BuildGUIDToLocalNodeMap().
  DCHECK_EQ(local_node->is_folder(), remote_node->is_folder);
  DCHECK_EQ(local_node->url(), remote_node->specifics.bookmark().url());
  return local_node;
}

}  // namespace sync_bookmarks
