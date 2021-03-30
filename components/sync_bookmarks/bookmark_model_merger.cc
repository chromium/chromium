// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/hash_util.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "ui/base/models/tree_node_iterator.h"

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

// Maximum depth to sync bookmarks tree to protect against stack overflow.
// Keep in sync with |base::internal::kAbsoluteMaxDepth| in json_common.h.
const size_t kMaxBookmarkTreeDepth = 200;

// The value must be a list since there is a container using pointers to its
// elements.
using UpdatesPerParentId =
    std::unordered_map<std::string, std::list<syncer::UpdateResponseData>>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When adding values, be certain to also
// update the corresponding definition in enums.xml and the
// ExpectedBookmarksGUIDDuplicates in unittests.
enum class BookmarksGUIDDuplicates {
  // Both entities are URLs with matching URLs in specifics. Entities may have
  // different titles or parents.
  kMatchingUrls = 0,
  // Both entities are folders with matching titles. Entities may have different
  // parents.
  kMatchingFolders = 1,
  // Both entities are URLs, but they have different URLs.
  kDifferentUrls = 2,
  // Both entities are folders with different titles.
  kDifferentFolders = 3,
  // Entities have different types.
  kDifferentTypes = 4,

  kMaxValue = kDifferentTypes,
};

// Used in metrics: "Sync.ProblematicServerSideBookmarksDuringMerge". These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class RemoteBookmarkUpdateError {
  // Invalid specifics.
  kInvalidSpecifics = 1,
  // Invalid unique position.
  kInvalidUniquePosition = 2,
  // Parent entity not found in server.
  kMissingParentEntity = 4,
  // The bookmark's GUID did not match the originator client item ID.
  kUnexpectedGuid = 9,
  // Parent is not a folder.
  kParentNotFolder = 10,

  kMaxValue = kParentNotFolder,
};

void LogProblematicBookmark(RemoteBookmarkUpdateError problem) {
  base::UmaHistogramEnumeration(
      "Sync.ProblematicServerSideBookmarksDuringMerge", problem);
}

void LogBookmarkReuploadNeeded(bool is_reupload_needed) {
  base::UmaHistogramBoolean("Sync.BookmarkEntityReuploadNeeded.OnInitialMerge",
                            is_reupload_needed);
}

// Gets the bookmark node corresponding to a permanent folder identified by
// |server_defined_unique_tag|. |bookmark_model| must not be null.
const bookmarks::BookmarkNode* GetPermanentFolder(
    const bookmarks::BookmarkModel* bookmark_model,
    const std::string& server_defined_unique_tag) {
  DCHECK(bookmark_model);

  if (server_defined_unique_tag == kBookmarkBarTag) {
    return bookmark_model->bookmark_bar_node();
  }
  if (server_defined_unique_tag == kOtherBookmarksTag) {
    return bookmark_model->other_node();
  }
  if (server_defined_unique_tag == kMobileBookmarksTag) {
    return bookmark_model->mobile_node();
  }

  return nullptr;
}

std::string LegacyCanonicalizedTitleFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics) {
  if (specifics.has_full_title()) {
    return FullTitleToLegacyCanonicalizedTitle(specifics.full_title());
  }
  return specifics.legacy_canonicalized_title();
}

// Heuristic to consider two nodes (local and remote) a match by semantics for
// the purpose of merging. Two folders match by semantics if they have the same
// title, two bookmarks match by semantics if they have the same title and url.
// A folder and a bookmark never match.
bool NodeSemanticsMatch(const bookmarks::BookmarkNode* local_node,
                        const std::string& remote_canonicalized_title,
                        const GURL& remote_url,
                        bool remote_is_folder) {
  if (local_node->is_folder() != remote_is_folder) {
    return false;
  }

  if (!remote_is_folder && local_node->url() != remote_url) {
    return false;
  }

  const std::string local_title = base::UTF16ToUTF8(local_node->GetTitle());
  // Titles match if they are identical or the remote one is the canonical form
  // of the local one. The latter is the case when a legacy client has
  // canonicalized the same local title before committing it. Modern clients
  // don't canonicalize titles anymore.
  // TODO(rushans): the comment above is off.
  if (local_title != remote_canonicalized_title &&
      sync_bookmarks::FullTitleToLegacyCanonicalizedTitle(local_title) !=
          remote_canonicalized_title) {
    return false;
  }

  return true;
}

BookmarksGUIDDuplicates MatchBookmarksGUIDDuplicates(
    const UpdateResponseData& update,
    const UpdateResponseData& duplicate_update) {
  if (update.entity.is_folder != duplicate_update.entity.is_folder) {
    return BookmarksGUIDDuplicates::kDifferentTypes;
  }
  if (update.entity.is_folder) {
    // Both entities are folders.
    const bool matching_titles =
        LegacyCanonicalizedTitleFromSpecifics(
            update.entity.specifics.bookmark()) ==
        LegacyCanonicalizedTitleFromSpecifics(
            duplicate_update.entity.specifics.bookmark());
    return matching_titles ? BookmarksGUIDDuplicates::kMatchingFolders
                           : BookmarksGUIDDuplicates::kDifferentFolders;
  }
  const bool matching_urls = update.entity.specifics.bookmark().url() ==
                             duplicate_update.entity.specifics.bookmark().url();
  return matching_urls ? BookmarksGUIDDuplicates::kMatchingUrls
                       : BookmarksGUIDDuplicates::kDifferentUrls;
}

void ReparentAllChildren(const std::string& from_parent_id,
                         const std::string& to_parent_id,
                         UpdatesPerParentId* updates_per_parent_id) {
  // Any of parents may be empty.
  auto from_parent_updates_iter = updates_per_parent_id->find(from_parent_id);
  if (from_parent_updates_iter == updates_per_parent_id->end()) {
    // There is nothing to merge.
    return;
  }

  // Update parent ids for all entities before moving.
  for (auto& update : from_parent_updates_iter->second) {
    DCHECK_EQ(update.entity.parent_id, from_parent_id);
    update.entity.parent_id = to_parent_id;
  }

  // Move all elements to a new parent (create one if it didn't exist).
  (*updates_per_parent_id)[to_parent_id].splice(
      (*updates_per_parent_id)[to_parent_id].end(),
      from_parent_updates_iter->second);
  updates_per_parent_id->erase(from_parent_id);

  // No need to update iterators since splice doesn't invalidate them.
}

// Returns true the |next_update| is selected to keep and the |previous_update|
// should be removed. False is returned otherwise. |next_update| and
// |previous_update| must have the same GUID.
bool CompareDuplicateUpdates(const UpdateResponseData& next_update,
                             const UpdateResponseData& previous_update) {
  DCHECK_EQ(next_update.entity.specifics.bookmark().guid(),
            previous_update.entity.specifics.bookmark().guid());
  DCHECK_NE(next_update.entity.id, previous_update.entity.id);

  if (next_update.entity.is_folder != previous_update.entity.is_folder) {
    // There are two entities, one of them is a folder and another one is a
    // URL. Prefer to save the folder as it may contain many bookmarks.
    return next_update.entity.is_folder;
  }
  // Choose the latest element to keep if both updates have the same type.
  return next_update.entity.creation_time >
         previous_update.entity.creation_time;
}

void DeduplicateValidUpdatesByGUID(UpdatesPerParentId* updates_per_parent_id) {
  DCHECK(updates_per_parent_id);

  std::unordered_map<base::GUID, std::list<UpdateResponseData>::iterator,
                     base::GUIDHash>
      guid_to_update;

  // Removing data in a separate loop helps easier merge parents since one of
  // them may have already been processed.
  std::list<std::list<UpdateResponseData>::iterator> updates_to_remove;
  for (auto& parent_id_and_updates : *updates_per_parent_id) {
    std::list<UpdateResponseData>* updates = &parent_id_and_updates.second;
    for (auto updates_iter = updates->begin(); updates_iter != updates->end();
         ++updates_iter) {
      const UpdateResponseData& update = *updates_iter;
      DCHECK(!update.entity.is_deleted());
      DCHECK(update.entity.server_defined_unique_tag.empty());

      const base::GUID guid_in_specifics =
          base::GUID::ParseLowercase(update.entity.specifics.bookmark().guid());
      DCHECK(guid_in_specifics.is_valid());

      auto it_and_success =
          guid_to_update.emplace(guid_in_specifics, updates_iter);
      if (it_and_success.second) {
        continue;
      }
      const UpdateResponseData& duplicate_update =
          *it_and_success.first->second;
      DCHECK_EQ(guid_in_specifics.AsLowercaseString(),
                duplicate_update.entity.specifics.bookmark().guid());
      DLOG(ERROR) << "Duplicate guid for new sync ID " << update.entity.id
                  << " and original sync ID " << duplicate_update.entity.id;
      const BookmarksGUIDDuplicates match_result =
          MatchBookmarksGUIDDuplicates(update, duplicate_update);
      base::UmaHistogramEnumeration("Sync.BookmarksGUIDDuplicates",
                                    match_result);

      if (CompareDuplicateUpdates(/*next_update=*/update,
                                  /*previous_update=*/duplicate_update)) {
        updates_to_remove.push_back(it_and_success.first->second);
        // Update |guid_to_update| to find a duplicate folder and merge them.
        guid_to_update[guid_in_specifics] = updates_iter;
      } else {
        updates_to_remove.push_back(updates_iter);
      }
    }
  }

  for (std::list<UpdateResponseData>::iterator updates_iter :
       updates_to_remove) {
    if (updates_iter->entity.is_folder) {
      const base::GUID guid = base::GUID::ParseLowercase(
          updates_iter->entity.specifics.bookmark().guid());
      DCHECK(base::Contains(guid_to_update, guid));
      DCHECK(guid_to_update[guid] != updates_iter);

      // Never remove a folder if its duplicate is a URL.
      DCHECK(guid_to_update[guid]->entity.is_folder);

      // Merge doesn't affect iterators.
      ReparentAllChildren(
          /*from_parent_id=*/updates_iter->entity.id,
          /*to_parent_id=*/guid_to_update[guid]->entity.id,
          updates_per_parent_id);
    }

    const std::string& parent_id = updates_iter->entity.parent_id;
    DCHECK(base::Contains(*updates_per_parent_id, parent_id));
    (*updates_per_parent_id)[parent_id].erase(updates_iter);
  }
}

// Checks that the |update| is valid and returns false otherwise. It is used to
// verify non-deletion updates. |update| must not be a deletion and a permanent
// node (they are processed in a different way).
bool IsValidUpdate(const UpdateResponseData& update) {
  const EntityData& update_entity = update.entity;

  DCHECK(!update_entity.is_deleted());
  DCHECK(update_entity.server_defined_unique_tag.empty());

  if (!syncer::UniquePosition::FromProto(update_entity.unique_position)
           .IsValid()) {
    // Ignore updates with invalid positions.
    DLOG(ERROR)
        << "Remote update with invalid position: "
        << update_entity.specifics.bookmark().legacy_canonicalized_title();
    LogProblematicBookmark(RemoteBookmarkUpdateError::kInvalidUniquePosition);
    return false;
  }
  if (!IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                update_entity.is_folder)) {
    // Ignore updates with invalid specifics.
    DLOG(ERROR) << "Remote update with invalid specifics";
    LogProblematicBookmark(RemoteBookmarkUpdateError::kInvalidSpecifics);
    return false;
  }
  if (!HasExpectedBookmarkGuid(update_entity.specifics.bookmark(),
                               update_entity.client_tag_hash,
                               update_entity.originator_cache_guid,
                               update_entity.originator_client_item_id)) {
    // Ignore updates with an unexpected GUID.
    DLOG(ERROR) << "Remote update with unexpected GUID";
    LogProblematicBookmark(RemoteBookmarkUpdateError::kUnexpectedGuid);
    return false;
  }
  return true;
}

struct GroupedUpdates {
  // |updates_per_parent_id| contains all valid updates grouped by their
  // |parent_id|. Permanent nodes and deletions are filtered out. Permanent
  // nodes are stored in a dedicated list |permanent_node_updates|.
  UpdatesPerParentId updates_per_parent_id;
  UpdateResponseDataList permanent_node_updates;
};

// Groups all valid updates by the server ID of their parent. Permanent nodes
// are grouped in a dedicated |permanent_node_updates| list in a returned value.
GroupedUpdates GroupValidUpdates(UpdateResponseDataList updates) {
  GroupedUpdates grouped_updates;
  for (UpdateResponseData& update : updates) {
    const EntityData& update_entity = update.entity;
    if (update_entity.is_deleted()) {
      continue;
    }
    if (!update_entity.server_defined_unique_tag.empty()) {
      grouped_updates.permanent_node_updates.push_back(std::move(update));
      continue;
    }
    if (!IsValidUpdate(update)) {
      continue;
    }
    grouped_updates.updates_per_parent_id[update_entity.parent_id].push_back(
        std::move(update));
  }

  return grouped_updates;
}

}  // namespace

BookmarkModelMerger::RemoteTreeNode::RemoteTreeNode() = default;

BookmarkModelMerger::RemoteTreeNode::~RemoteTreeNode() = default;

BookmarkModelMerger::RemoteTreeNode::RemoteTreeNode(
    BookmarkModelMerger::RemoteTreeNode&&) = default;
BookmarkModelMerger::RemoteTreeNode& BookmarkModelMerger::RemoteTreeNode::
operator=(BookmarkModelMerger::RemoteTreeNode&&) = default;

void BookmarkModelMerger::RemoteTreeNode::EmplaceSelfAndDescendantsByGUID(
    std::unordered_map<base::GUID, const RemoteTreeNode*, base::GUIDHash>*
        guid_to_remote_node_map) const {
  DCHECK(guid_to_remote_node_map);

  if (entity().server_defined_unique_tag.empty()) {
    const base::GUID guid =
        base::GUID::ParseLowercase(entity().specifics.bookmark().guid());
    DCHECK(guid.is_valid());

    // Duplicate GUIDs have been sorted out before.
    bool success = guid_to_remote_node_map->emplace(guid, this).second;
    DCHECK(success);
  }

  for (const RemoteTreeNode& child : children_) {
    child.EmplaceSelfAndDescendantsByGUID(guid_to_remote_node_map);
  }
}

// static
bool BookmarkModelMerger::RemoteTreeNode::UniquePositionLessThan(
    const RemoteTreeNode& lhs,
    const RemoteTreeNode& rhs) {
  const syncer::UniquePosition a_pos =
      syncer::UniquePosition::FromProto(lhs.entity().unique_position);
  const syncer::UniquePosition b_pos =
      syncer::UniquePosition::FromProto(rhs.entity().unique_position);
  return a_pos.LessThan(b_pos);
}

// static
BookmarkModelMerger::RemoteTreeNode
BookmarkModelMerger::RemoteTreeNode::BuildTree(
    UpdateResponseData update,
    size_t max_depth,
    UpdatesPerParentId* updates_per_parent_id) {
  DCHECK(updates_per_parent_id);

  RemoteTreeNode node;
  node.update_ = std::move(update);

  // Ensure we have not reached the maximum tree depth to guard against stack
  // overflows.
  if (max_depth == 0) {
    return node;
  }

  // Only folders may have descendants (ignore them otherwise). Treat
  // permanent nodes as folders explicitly.
  if (!node.update_.entity.is_folder &&
      node.update_.entity.server_defined_unique_tag.empty()) {
    // Children of a non-folder are ignored.
    for (UpdateResponseData& child_update :
         (*updates_per_parent_id)[node.entity().id]) {
      LogProblematicBookmark(RemoteBookmarkUpdateError::kParentNotFolder);
      // To avoid double-counting later for bucket |kMissingParentEntity|,
      // clear the update from the list as if it would have been moved.
      child_update.entity = EntityData();
    }
    return node;
  }

  // Populate descendants recursively.
  for (UpdateResponseData& child_update :
       (*updates_per_parent_id)[node.entity().id]) {
    DCHECK_EQ(child_update.entity.parent_id, node.entity().id);
    DCHECK(IsValidBookmarkSpecifics(child_update.entity.specifics.bookmark(),
                                    child_update.entity.is_folder));

    node.children_.push_back(BuildTree(std::move(child_update), max_depth - 1,
                                       updates_per_parent_id));
  }

  // Sort the children according to their unique position.
  std::stable_sort(node.children_.begin(), node.children_.end(),
                   UniquePositionLessThan);

  return node;
}

BookmarkModelMerger::BookmarkModelMerger(
    UpdateResponseDataList updates,
    bookmarks::BookmarkModel* bookmark_model,
    favicon::FaviconService* favicon_service,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_model_(bookmark_model),
      favicon_service_(favicon_service),
      bookmark_tracker_(bookmark_tracker),
      remote_forest_(BuildRemoteForest(std::move(updates))),
      guid_to_match_map_(
          FindGuidMatchesOrReassignLocal(remote_forest_, bookmark_model_)) {
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
  for (const auto& tree_tag_and_root : remote_forest_) {
    const bookmarks::BookmarkNode* permanent_folder =
        GetPermanentFolder(bookmark_model_, tree_tag_and_root.first);
    if (!permanent_folder) {
      continue;
    }
    MergeSubtree(/*local_subtree_root=*/permanent_folder,
                 /*remote_node=*/tree_tag_and_root.second);
  }

  if (base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarkFullTitles)) {
    // When the reupload feature is enabled, all new empty trackers are
    // automatically reuploaded (since there are no entities to reupload). This
    // is used to disable reupload after initial merge.
    bookmark_tracker_->SetBookmarksFullTitleReuploaded();
  }
}

// static
BookmarkModelMerger::RemoteForest BookmarkModelMerger::BuildRemoteForest(
    syncer::UpdateResponseDataList updates) {
  // Filter out invalid remote updates and group the valid ones by the server ID
  // of their parent.
  GroupedUpdates grouped_updates = GroupValidUpdates(std::move(updates));

  DeduplicateValidUpdatesByGUID(&grouped_updates.updates_per_parent_id);

  // Construct one tree per permanent entity.
  RemoteForest update_forest;
  for (UpdateResponseData& permanent_node_update :
       grouped_updates.permanent_node_updates) {
    // Make a copy of the string to avoid relying on argument evaluation order.
    const std::string server_defined_unique_tag =
        permanent_node_update.entity.server_defined_unique_tag;
    DCHECK(!server_defined_unique_tag.empty());

    update_forest.emplace(
        server_defined_unique_tag,
        RemoteTreeNode::BuildTree(std::move(permanent_node_update),
                                  kMaxBookmarkTreeDepth,
                                  &grouped_updates.updates_per_parent_id));
  }

  // All remaining entries in |updates_per_parent_id| must be unreachable from
  // permanent entities, since otherwise they would have been moved away.
  for (const auto& parent_id_and_updates :
       grouped_updates.updates_per_parent_id) {
    for (const UpdateResponseData& update : parent_id_and_updates.second) {
      if (!update.entity.is_deleted() &&
          update.entity.specifics.has_bookmark()) {
        LogProblematicBookmark(RemoteBookmarkUpdateError::kMissingParentEntity);
      }
    }
  }

  return update_forest;
}

// static
std::unordered_map<base::GUID, BookmarkModelMerger::GuidMatch, base::GUIDHash>
BookmarkModelMerger::FindGuidMatchesOrReassignLocal(
    const RemoteForest& remote_forest,
    bookmarks::BookmarkModel* bookmark_model) {
  DCHECK(bookmark_model);

  // Build a temporary lookup table for remote GUIDs.
  std::unordered_map<base::GUID, const RemoteTreeNode*, base::GUIDHash>
      guid_to_remote_node_map;
  for (const auto& tree_tag_and_root : remote_forest) {
    tree_tag_and_root.second.EmplaceSelfAndDescendantsByGUID(
        &guid_to_remote_node_map);
  }

  // Iterate through all local bookmarks to find matches by GUID.
  std::unordered_map<base::GUID, BookmarkModelMerger::GuidMatch, base::GUIDHash>
      guid_to_match_map;
  // Because ReplaceBookmarkNodeGUID() cannot be used while iterating the local
  // bookmark model, a temporary list is constructed first to reassign later.
  std::vector<const bookmarks::BookmarkNode*> nodes_to_replace_guid;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmark_model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* const node = iterator.Next();
    DCHECK(node->guid().is_valid());

    const auto remote_it = guid_to_remote_node_map.find(node->guid());
    if (remote_it == guid_to_remote_node_map.end()) {
      continue;
    }

    const RemoteTreeNode* const remote_node = remote_it->second;
    const syncer::EntityData& remote_entity = remote_node->entity();

    // Permanent nodes don't match by GUID but by |server_defined_unique_tag|.
    // As extra precaution, specially with remote GUIDs in mind, let's ignore
    // them explicitly here.
    if (node->is_permanent_node() ||
        GetPermanentFolder(bookmark_model,
                           remote_entity.server_defined_unique_tag) !=
            nullptr) {
      continue;
    }

    if (node->is_folder() != remote_entity.is_folder ||
        (node->is_url() &&
         node->url() != remote_entity.specifics.bookmark().url())) {
      // If local node and its remote node match are conflicting in node type or
      // URL, replace local GUID with a random GUID.
      nodes_to_replace_guid.push_back(node);
      continue;
    }

    bool success =
        guid_to_match_map.emplace(node->guid(), GuidMatch{node, remote_node})
            .second;

    // Insertion must have succeeded unless there were duplicate GUIDs in the
    // local BookmarkModel (invariant violation that gets resolved upon
    // restart).
    // TODO(crbug.com/516866): The below CHECK is added to debug some crashes.
    // Should be converted to a DCHECK after the root cause if found.
    CHECK(success);
  }

  for (const bookmarks::BookmarkNode* node : nodes_to_replace_guid) {
    ReplaceBookmarkNodeGUID(node, base::GUID::GenerateRandomV4(),
                            bookmark_model);
  }

  return guid_to_match_map;
}

void BookmarkModelMerger::MergeSubtree(
    const bookmarks::BookmarkNode* local_subtree_root,
    const RemoteTreeNode& remote_node) {
  const EntityData& remote_update_entity = remote_node.entity();
  const SyncedBookmarkTracker::Entity* entity = bookmark_tracker_->Add(
      local_subtree_root, remote_update_entity.id,
      remote_node.response_version(), remote_update_entity.creation_time,
      remote_update_entity.unique_position, remote_update_entity.specifics);
  const bool is_reupload_needed =
      !local_subtree_root->is_permanent_node() &&
      IsBookmarkEntityReuploadNeeded(remote_update_entity);
  if (is_reupload_needed) {
    bookmark_tracker_->IncrementSequenceNumber(entity);
  }
  LogBookmarkReuploadNeeded(is_reupload_needed);

  // If there are remote child updates, try to match them.
  for (size_t remote_index = 0; remote_index < remote_node.children().size();
       ++remote_index) {
    // TODO(crbug.com/1050776): change to DCHECK after investigating.
    // Here is expected that all nodes to the left of current |remote_index| are
    // filled with remote updates. All local nodes which are not merged will be
    // added later.
    CHECK_LE(remote_index, local_subtree_root->children().size());
    const RemoteTreeNode& remote_child =
        remote_node.children().at(remote_index);
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

  // At this point all the children of |remote_node| have corresponding local
  // nodes under |local_subtree_root| and they are all in the right positions:
  // from 0 to remote_node.children().size() - 1.
  //
  // This means, the children starting from remote_node.children().size() in
  // the parent bookmark node are the ones that are not present in the parent
  // sync node and not tracked yet. So create all of the remaining local nodes.
  DCHECK_LE(remote_node.children().size(),
            local_subtree_root->children().size());

  for (size_t i = remote_node.children().size();
       i < local_subtree_root->children().size(); ++i) {
    // If local node has been or will be matched by GUID, skip it.
    if (FindMatchingRemoteNodeByGUID(local_subtree_root->children()[i].get())) {
      continue;
    }
    ProcessLocalCreation(local_subtree_root, i);
  }
}

const bookmarks::BookmarkNode* BookmarkModelMerger::FindMatchingLocalNode(
    const RemoteTreeNode& remote_child,
    const bookmarks::BookmarkNode* local_parent,
    size_t local_child_start_index) const {
  DCHECK(local_parent);
  // Try to match child by GUID. If we can't, try to match child by semantics.
  const bookmarks::BookmarkNode* matching_local_node_by_guid =
      FindMatchingLocalNodeByGUID(remote_child);
  if (matching_local_node_by_guid) {
    return matching_local_node_by_guid;
  }

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

  // The child at |local_index| has matched by semantics, which also means it
  // does not match by GUID to any other remote node.
  const bookmarks::BookmarkNode* matching_local_node_by_semantics =
      local_parent->children()[local_index].get();
  DCHECK(!FindMatchingRemoteNodeByGUID(matching_local_node_by_semantics));
  return matching_local_node_by_semantics;
}

const bookmarks::BookmarkNode*
BookmarkModelMerger::UpdateBookmarkNodeFromSpecificsIncludingGUID(
    const bookmarks::BookmarkNode* local_node,
    const RemoteTreeNode& remote_node) {
  DCHECK(local_node);
  DCHECK(!local_node->is_permanent_node());
  // Ensure bookmarks have the same URL, otherwise they would not have been
  // matched.
  DCHECK(local_node->is_folder() ||
         local_node->url() ==
             GURL(remote_node.entity().specifics.bookmark().url()));
  const EntityData& remote_update_entity = remote_node.entity();
  const sync_pb::BookmarkSpecifics& specifics =
      remote_update_entity.specifics.bookmark();

  // Update the local GUID if necessary for semantic matches (it's obviously not
  // needed for GUID-based matches).
  const bookmarks::BookmarkNode* possibly_replaced_local_node = local_node;
  if (!specifics.guid().empty() &&
      specifics.guid() != local_node->guid().AsLowercaseString()) {
    // If it's a semantic match, neither of the nodes should be involved in any
    // GUID-based match.
    DCHECK(!FindMatchingLocalNodeByGUID(remote_node));
    DCHECK(!FindMatchingRemoteNodeByGUID(local_node));

    possibly_replaced_local_node = ReplaceBookmarkNodeGUID(
        local_node, base::GUID::ParseLowercase(specifics.guid()),
        bookmark_model_);

    // TODO(rushans): remove the code below since DCHECKs above guarantee that
    // |guid_to_match_map_| has no such GUID.
    //
    // Update |guid_to_match_map_| to avoid pointing to a deleted node. This
    // should not be required in practice, because the algorithm processes each
    // GUID once, but let's update nevertheless to avoid future issues.
    const auto it =
        guid_to_match_map_.find(possibly_replaced_local_node->guid());
    if (it != guid_to_match_map_.end() && it->second.local_node == local_node) {
      it->second.local_node = possibly_replaced_local_node;
    }
  }

  // Update all fields, where no-op changes are handled well.
  UpdateBookmarkNodeFromSpecifics(specifics, possibly_replaced_local_node,
                                  bookmark_model_, favicon_service_);

  return possibly_replaced_local_node;
}

void BookmarkModelMerger::ProcessRemoteCreation(
    const RemoteTreeNode& remote_node,
    const bookmarks::BookmarkNode* local_parent,
    size_t index) {
  DCHECK(!FindMatchingLocalNodeByGUID(remote_node));

  const EntityData& remote_update_entity = remote_node.entity();
  DCHECK(IsValidBookmarkSpecifics(remote_update_entity.specifics.bookmark(),
                                  remote_update_entity.is_folder));

  const sync_pb::EntitySpecifics& specifics = remote_node.entity().specifics;
  const bookmarks::BookmarkNode* bookmark_node =
      CreateBookmarkNodeFromSpecifics(specifics.bookmark(), local_parent, index,
                                      remote_update_entity.is_folder,
                                      bookmark_model_, favicon_service_);
  DCHECK(bookmark_node);
  const SyncedBookmarkTracker::Entity* entity = bookmark_tracker_->Add(
      bookmark_node, remote_update_entity.id, remote_node.response_version(),
      remote_update_entity.creation_time, remote_update_entity.unique_position,
      specifics);
  const bool is_reupload_needed =
      IsBookmarkEntityReuploadNeeded(remote_node.entity());
  if (is_reupload_needed) {
    bookmark_tracker_->IncrementSequenceNumber(entity);
  }
  LogBookmarkReuploadNeeded(is_reupload_needed);

  // Recursively, match by GUID or, if not possible, create local node for all
  // child remote nodes.
  size_t i = 0;
  for (const RemoteTreeNode& remote_child : remote_node.children()) {
    // TODO(crbug.com/1050776): change to DCHECK after investigating of some
    // crashes.
    CHECK_LE(i, bookmark_node->children().size());
    const bookmarks::BookmarkNode* local_child =
        FindMatchingLocalNodeByGUID(remote_child);
    if (!local_child) {
      ProcessRemoteCreation(remote_child, bookmark_node, i++);
      continue;
    }
    bookmark_model_->Move(local_child, bookmark_node, i++);
    local_child =
        UpdateBookmarkNodeFromSpecificsIncludingGUID(local_child, remote_child);
    MergeSubtree(local_child, remote_child);
  }
}

void BookmarkModelMerger::ProcessLocalCreation(
    const bookmarks::BookmarkNode* parent,
    size_t index) {
  DCHECK_LE(index, parent->children().size());
  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForBookmarkNode(parent);
  // Since we are merging top down, parent entity must be tracked.
  DCHECK(parent_entity);

  // Assign a temp server id for the entity. Will be overridden by the actual
  // server id upon receiving commit response.
  const bookmarks::BookmarkNode* node = parent->children()[index].get();
  DCHECK(!FindMatchingRemoteNodeByGUID(node));

  // The node's GUID cannot run into collisions because
  // FindGuidMatchesOrReassignLocal() takes care of reassigning local GUIDs if
  // they won't actually be merged with the remote bookmark with the same GUID
  // (e.g. incompatible types).
  const std::string sync_id = node->guid().AsLowercaseString();
  const int64_t server_version = syncer::kUncommittedVersion;
  const base::Time creation_time = base::Time::Now();
  const std::string& suffix = syncer::GenerateSyncableBookmarkHash(
      bookmark_tracker_->model_type_state().cache_guid(), sync_id);
  // Locally created nodes aren't tracked and hence don't have a unique position
  // yet so we need to produce new ones.
  const syncer::UniquePosition pos =
      GenerateUniquePositionForLocalCreation(parent, index, suffix);
  const sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, /*force_favicon_load=*/true);
  const SyncedBookmarkTracker::Entity* entity = bookmark_tracker_->Add(
      node, sync_id, server_version, creation_time, pos.ToProto(), specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
  for (size_t i = 0; i < node->children().size(); ++i) {
    // If a local node hasn't matched with any remote entity, its descendants
    // will neither, unless they have been or will be matched by GUID, in which
    // case we skip them for now.
    if (FindMatchingRemoteNodeByGUID(node->children()[i].get())) {
      continue;
    }
    ProcessLocalCreation(/*parent=*/node, i);
  }
}

size_t BookmarkModelMerger::FindMatchingChildBySemanticsStartingAt(
    const RemoteTreeNode& remote_node,
    const bookmarks::BookmarkNode* local_parent,
    size_t starting_child_index) const {
  DCHECK(local_parent);
  const auto& children = local_parent->children();
  DCHECK_LE(starting_child_index, children.size());
  const EntityData& remote_entity = remote_node.entity();

  // Precompute the remote title and URL before searching for a matching local
  // node.
  const std::string remote_canonicalized_title =
      LegacyCanonicalizedTitleFromSpecifics(remote_entity.specifics.bookmark());
  const bool remote_is_folder = remote_entity.is_folder;
  GURL remote_url;
  if (!remote_entity.is_folder) {
    remote_url = GURL(remote_entity.specifics.bookmark().url());
  }
  const auto it = std::find_if(
      children.cbegin() + starting_child_index, children.cend(),
      [this, &remote_canonicalized_title, &remote_url,
       remote_is_folder](const auto& child) {
        return !FindMatchingRemoteNodeByGUID(child.get()) &&
               NodeSemanticsMatch(child.get(), remote_canonicalized_title,
                                  remote_url, remote_is_folder);
      });
  return (it == children.cend()) ? kInvalidIndex : (it - children.cbegin());
}

const BookmarkModelMerger::RemoteTreeNode*
BookmarkModelMerger::FindMatchingRemoteNodeByGUID(
    const bookmarks::BookmarkNode* local_node) const {
  DCHECK(local_node);

  const auto it = guid_to_match_map_.find(local_node->guid());
  if (it == guid_to_match_map_.end()) {
    return nullptr;
  }

  DCHECK_EQ(it->second.local_node, local_node);
  return it->second.remote_node;
}

const bookmarks::BookmarkNode* BookmarkModelMerger::FindMatchingLocalNodeByGUID(
    const RemoteTreeNode& remote_node) const {
  const syncer::EntityData& remote_entity = remote_node.entity();
  const auto it = guid_to_match_map_.find(
      base::GUID::ParseLowercase(remote_entity.specifics.bookmark().guid()));
  if (it == guid_to_match_map_.end()) {
    return nullptr;
  }

  DCHECK_EQ(it->second.remote_node, &remote_node);
  return it->second.local_node;
}

syncer::UniquePosition
BookmarkModelMerger::GenerateUniquePositionForLocalCreation(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::string& suffix) const {
  // Try to find last tracked preceding entity. It is not always the previous
  // one as it might be skipped if it has unprocessed remote matching by GUID
  // update.
  for (size_t i = index; i > 0; --i) {
    const SyncedBookmarkTracker::Entity* predecessor_entity =
        bookmark_tracker_->GetEntityForBookmarkNode(
            parent->children()[i - 1].get());
    if (predecessor_entity != nullptr) {
      return syncer::UniquePosition::After(
          syncer::UniquePosition::FromProto(
              predecessor_entity->metadata()->unique_position()),
          suffix);
    }
    DCHECK(FindMatchingRemoteNodeByGUID(parent->children()[i - 1].get()));
  }
  return syncer::UniquePosition::InitialPosition(suffix);
}

}  // namespace sync_bookmarks
