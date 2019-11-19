// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "components/sync/engine/non_blocking_sync_common.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace sync_bookmarks {

class SyncedBookmarkTracker;

// Responsible for merging local and remote bookmark models when bookmark sync
// is enabled for the first time by the user (i.e. no sync metadata exists
// locally), so we need a best-effort merge based on similarity. It implements
// similar logic to that in BookmarkModelAssociator::AssociateModels() to be
// used by the BookmarkModelTypeProcessor().
class BookmarkModelMerger {
 public:
  // |bookmark_model|, |favicon_service| and |bookmark_tracker| must not be
  // null and must outlive this object.
  BookmarkModelMerger(syncer::UpdateResponseDataList updates,
                      bookmarks::BookmarkModel* bookmark_model,
                      favicon::FaviconService* favicon_service,
                      SyncedBookmarkTracker* bookmark_tracker);

  ~BookmarkModelMerger();

  // Merges the remote bookmark model represented as the |updates| received from
  // the sync server and local bookmark model |bookmark_model|, and updates the
  // model and |bookmark_tracker| (all of which are injected in the constructor)
  // accordingly. On return, there will be a 1:1 mapping between bookmark nodes
  // and metadata entities in the injected tracker.
  void Merge();

 private:
  // Merges a local and a remote subtrees. The input nodes are two equivalent
  // local and remote nodes. This method tries to recursively match their
  // children. It updates the |bookmark_tracker_| accordingly.
  void MergeSubtree(const bookmarks::BookmarkNode* local_node,
                    const syncer::UpdateResponseData* remote_update);

  // Updates |local_node| to hold same GUID and semantics as its |remote_update|
  // match. The input nodes are two equivalent local and remote bookmarks that
  // are about to be merged. The output node is the potentially replaced
  // |local_node|. |local_node| must not be a BookmarkPermanentNode.
  const bookmarks::BookmarkNode* UpdateBookmarkNodeFromSpecificsIncludingGUID(
      const bookmarks::BookmarkNode* local_node,
      const syncer::UpdateResponseData* remote_update);

  // Creates a local bookmark node for a |remote_update|. The local node is
  // created under |local_parent| at position |index|. If the remote node has
  // children, this method recursively creates them as well. It updates the
  // |bookmark_tracker_| accordingly.
  void ProcessRemoteCreation(const syncer::UpdateResponseData* remote_update,
                             const bookmarks::BookmarkNode* local_parent,
                             size_t index);

  // Creates a server counter-part for the local node at position |index|
  // under |parent|. If the local node has children, corresponding server nodes
  // are created recursively. It updates the |bookmark_tracker_| accordingly and
  // new nodes are marked to be committed.
  void ProcessLocalCreation(const bookmarks::BookmarkNode* parent,
                            size_t index);

  // Gets the bookmark node corresponding to a permanent folder.
  // |update_entity| must contain server_defined_unique_tag that is used to
  // determine the corresponding permanent node.
  const bookmarks::BookmarkNode* GetPermanentFolder(
      const syncer::EntityData& update_entity) const;

  // Looks for a local node under |local_subtree_root| that matches
  // |remote_child|, starting at index |remote_index|. First attempts to find a
  // match by GUID and otherwise attempts to find one by semantics. If no match
  // is found, a nullptr is returned.
  const bookmarks::BookmarkNode* FindMatchingLocalNode(
      const syncer::UpdateResponseData* remote_child,
      const bookmarks::BookmarkNode* local_parent,
      size_t local_child_start_index) const;

  // If |local_node| has a remote counterpart of the same GUID, returns the
  // corresponding remote update, otherwise returns a nullptr. |local_node| must
  // not be null.
  const syncer::UpdateResponseData* FindMatchingRemoteUpdateByGUID(
      const bookmarks::BookmarkNode* local_node) const;

  // If |remote_update| has a local counterpart of the same GUID, returns the
  // corresponding local node, otherwise returns a nullptr. |remote_update| must
  // not be null.
  const bookmarks::BookmarkNode* FindMatchingLocalNodeByGUID(
      const syncer::UpdateResponseData& remote_update) const;

  // Tries to find a child local node under |local_parent| that matches
  // |remote_node| semantically and returns the index of that node, as long as
  // this local child cannot be matched by GUID to a different node. Matching is
  // decided using NodeSemanticsMatch(). It searches in the children list
  // starting from position |search_starting_child_index|. In case of no match
  // is found, it returns |kInvalidIndex|.
  size_t FindMatchingChildBySemanticsStartingAt(
      const syncer::UpdateResponseData* remote_node,
      const bookmarks::BookmarkNode* local_parent,
      size_t starting_child_index) const;

  // Original updates as passed in the constructor, which may contain invalid
  // updates. Needed to hold ownership of updates (other data structures such as
  // |updates_tree_| point to these instances.
  const syncer::UpdateResponseDataList original_updates_;

  bookmarks::BookmarkModel* const bookmark_model_;
  favicon::FaviconService* const favicon_service_;
  SyncedBookmarkTracker* const bookmark_tracker_;
  // Stores the tree of |updates_| as a map from a remote node to a
  // vector of remote children. It's constructed in the c'tor.
  const std::unordered_map<const syncer::UpdateResponseData*,
                           std::vector<const syncer::UpdateResponseData*>>
      updates_tree_;
  // Maps GUIDs to their respective remote updates. Used for GUID-based
  // node matching and is populated at the beginning of the merge process.
  const std::unordered_map<std::string, const syncer::UpdateResponseData*>
      guid_to_remote_update_map_;
  // Maps GUIDs to their respective local nodes. Used for GUID-based
  // node matching and is populated at the beginning of the merge process.
  // Is not updated upon potential changes to the model during merge.
  const std::unordered_map<std::string, const bookmarks::BookmarkNode*>
      guid_to_local_node_map_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelMerger);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_H_
