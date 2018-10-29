// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_change_processor.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/containers/stack.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/favicon/core/favicon_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/syncable/change_record.h"
#include "components/sync/syncable/entry.h"  // TODO(tim): Investigating bug 121587.
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"
#include "components/undo/bookmark_undo_service.h"
#include "components/undo/bookmark_undo_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using syncer::ChangeRecord;
using syncer::ChangeRecordList;

namespace sync_bookmarks {

static const char kMobileBookmarksTag[] = "synced_bookmarks";

BookmarkChangeProcessor::BookmarkChangeProcessor(
    syncer::SyncClient* sync_client,
    BookmarkModelAssociator* model_associator,
    std::unique_ptr<syncer::DataTypeErrorHandler> err_handler)
    : syncer::ChangeProcessor(std::move(err_handler)),
      bookmark_model_(nullptr),
      sync_client_(sync_client),
      model_associator_(model_associator) {
  DCHECK(model_associator);
  DCHECK(sync_client);
  DCHECK(error_handler());
}

BookmarkChangeProcessor::~BookmarkChangeProcessor() {
  if (bookmark_model_)
    bookmark_model_->RemoveObserver(this);
}

void BookmarkChangeProcessor::StartImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bookmark_model_);
  bookmark_model_ = sync_client_->GetBookmarkModel();
  DCHECK(bookmark_model_->loaded());
  bookmark_model_->AddObserver(this);
}

void BookmarkChangeProcessor::UpdateSyncNodeProperties(
    const BookmarkNode* src,
    BookmarkModel* model,
    syncer::WriteNode* dst,
    syncer::DataTypeErrorHandler* error_handler) {
  // Set the properties of the item.
  dst->SetIsFolder(src->is_folder());
  dst->SetTitle(base::UTF16ToUTF8(src->GetTitle()));
  sync_pb::BookmarkSpecifics bookmark_specifics(dst->GetBookmarkSpecifics());
  if (!src->is_folder()) {
    if (!src->url().is_valid()) {
      // Report the invalid URL and continue.
      // TODO(stanisc): crbug/482155: Revisit this once the root cause for
      // invalid URLs is understood.
      error_handler->CreateAndUploadError(
          FROM_HERE, "Creating sync bookmark with invalid url " +
                         src->url().possibly_invalid_spec(),
          syncer::BOOKMARKS);
    }
    bookmark_specifics.set_url(src->url().spec());
  }
  bookmark_specifics.set_creation_time_us(src->date_added().ToInternalValue());
  dst->SetBookmarkSpecifics(bookmark_specifics);
  SetSyncNodeFavicon(src, model, dst);
  SetSyncNodeMetaInfo(src, dst);
}

// static
void BookmarkChangeProcessor::EncodeFavicon(
    const BookmarkNode* src,
    BookmarkModel* model,
    scoped_refptr<base::RefCountedMemory>* dst) {
  const gfx::Image& favicon = model->GetFavicon(src);

  // Check for empty images.  This can happen if the favicon is
  // still being loaded.  Also avoid syncing touch icons.
  if (favicon.IsEmpty() ||
      model->GetFaviconType(src) != favicon_base::IconType::kFavicon)
    return;

  // Re-encode the BookmarkNode's favicon as a PNG, and pass the data to the
  // sync subsystem.
  *dst = favicon.As1xPNGBytes();
}

// static
int BookmarkChangeProcessor::RemoveSyncNodeHierarchy(
    syncer::WriteTransaction* trans,
    syncer::WriteNode* sync_node,
    BookmarkModelAssociator* associator) {
  // Remove children.
  int num_removed = RemoveAllChildNodes(trans, sync_node->GetId(), associator);
  // Remove the node itself.
  RemoveOneSyncNode(sync_node, associator);
  return num_removed + 1;
}

void BookmarkChangeProcessor::RemoveSyncNodeHierarchy(
    const BookmarkNode* topmost) {
  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  {
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);
    syncer::WriteNode topmost_sync_node(&trans);
    if (!model_associator_->InitSyncNodeFromChromeId(topmost->id(),
                                                     &topmost_sync_node)) {
      syncer::SyncError error(FROM_HERE,
                              syncer::SyncError::DATATYPE_ERROR,
                              "Failed to init sync node from chrome node",
                              syncer::BOOKMARKS);
      error_handler()->OnUnrecoverableError(error);
      return;
    }
    RemoveSyncNodeHierarchy(&trans, &topmost_sync_node, model_associator_);
  }

  // Don't need to update versions of deleted nodes.
  UpdateTransactionVersion(new_version, bookmark_model_,
                           std::vector<const BookmarkNode*>());
}

void BookmarkChangeProcessor::RemoveAllSyncNodes() {
  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  {
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);

    int64_t bookmark_bar_node_sync_id =
        model_associator_->GetSyncIdFromChromeId(
            bookmark_model_->bookmark_bar_node()->id());
    DCHECK_NE(syncer::kInvalidId, bookmark_bar_node_sync_id);
    RemoveAllChildNodes(&trans, bookmark_bar_node_sync_id, model_associator_);

    int64_t other_node_sync_id = model_associator_->GetSyncIdFromChromeId(
        bookmark_model_->other_node()->id());
    DCHECK_NE(syncer::kInvalidId, other_node_sync_id);
    RemoveAllChildNodes(&trans, other_node_sync_id, model_associator_);

    // Remove mobile bookmarks node only if it is present.
    int64_t mobile_node_sync_id = model_associator_->GetSyncIdFromChromeId(
        bookmark_model_->mobile_node()->id());
    if (mobile_node_sync_id != syncer::kInvalidId) {
      RemoveAllChildNodes(&trans, mobile_node_sync_id, model_associator_);
    }

    // Note: the root node may have additional extra nodes. Currently none of
    // them are meant to sync.
  }

  // Don't need to update versions of deleted nodes.
  UpdateTransactionVersion(new_version, bookmark_model_,
                           std::vector<const BookmarkNode*>());
}

// static
int BookmarkChangeProcessor::RemoveAllChildNodes(
    syncer::WriteTransaction* trans,
    int64_t topmost_sync_id,
    BookmarkModelAssociator* associator) {
  // Do a DFS and delete all the child sync nodes, use sync id instead of
  // bookmark node ids since the bookmark nodes may already be deleted.
  // The equivalent recursive version of this iterative DFS:
  // remove_all_children(node_id, topmost_node_id):
  //    node.initByIdLookup(node_id)
  //    while(node.GetFirstChildId() != syncer::kInvalidId)
  //      remove_all_children(node.GetFirstChildId(), topmost_node_id)
  //    if(node_id != topmost_node_id)
  //      delete node

  int num_removed = 0;
  base::stack<int64_t> dfs_sync_id_stack;
  // Push the topmost node.
  dfs_sync_id_stack.push(topmost_sync_id);
  while (!dfs_sync_id_stack.empty()) {
    const int64_t sync_node_id = dfs_sync_id_stack.top();
    syncer::WriteNode node(trans);
    node.InitByIdLookup(sync_node_id);
    if (!node.GetIsFolder() || node.GetFirstChildId() == syncer::kInvalidId) {
      // All children of the node has been processed, delete the node and
      // pop it off the stack.
      dfs_sync_id_stack.pop();
      // Do not delete the topmost node.
      if (sync_node_id != topmost_sync_id) {
        RemoveOneSyncNode(&node, associator);
        num_removed++;
      } else {
        // if we are processing topmost node, all other nodes must be processed
        // the stack should be empty.
        DCHECK(dfs_sync_id_stack.empty());
      }
    } else {
      int64_t child_id = node.GetFirstChildId();
      if (child_id != syncer::kInvalidId) {
        dfs_sync_id_stack.push(child_id);
      }
    }
  }
  return num_removed;
}

// static
void BookmarkChangeProcessor::RemoveOneSyncNode(
    syncer::WriteNode* sync_node,
    BookmarkModelAssociator* associator) {
  // This node should have no children.
  DCHECK(!sync_node->HasChildren());
  // Remove association and delete the sync node.
  associator->Disassociate(sync_node->GetId());
  sync_node->Tombstone();
}

void BookmarkChangeProcessor::CreateOrUpdateSyncNode(const BookmarkNode* node) {
  if (!CanSyncNode(node)) {
    NOTREACHED();
    return;
  }

  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  int64_t sync_id = syncer::kInvalidId;
  {
    // Acquire a scoped write lock via a transaction.
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);
    sync_id = model_associator_->GetSyncIdFromChromeId(node->id());
    if (sync_id != syncer::kInvalidId) {
      UpdateSyncNode(
          node, bookmark_model_, &trans, model_associator_, error_handler());
    } else {
      const BookmarkNode* parent = node->parent();
      int index = parent->GetIndexOf(node);
      sync_id = CreateSyncNode(parent,
                               bookmark_model_,
                               index,
                               &trans,
                               model_associator_,
                               error_handler());
    }
  }

  if (syncer::kInvalidId != sync_id) {
    // Siblings of added node in sync DB will also be updated to reflect new
    // PREV_ID/NEXT_ID and thus get a new version. But we only update version
    // of added node here. After switching to ordinals for positioning,
    // PREV_ID/NEXT_ID will be deprecated and siblings will not be updated.
    UpdateTransactionVersion(new_version, bookmark_model_,
                             std::vector<const BookmarkNode*>(1, node));
  }
}

void BookmarkChangeProcessor::BookmarkModelLoaded(BookmarkModel* model,
                                                  bool ids_reassigned) {
  NOTREACHED();
}

void BookmarkChangeProcessor::BookmarkModelBeingDeleted(BookmarkModel* model) {
  NOTREACHED();
  bookmark_model_ = nullptr;
}

void BookmarkChangeProcessor::BookmarkNodeAdded(BookmarkModel* model,
                                                const BookmarkNode* parent,
                                                int index) {
  DCHECK(share_handle());
  const BookmarkNode* node = parent->GetChild(index);
  if (CanSyncNode(node))
    CreateOrUpdateSyncNode(node);
}

// static
int64_t BookmarkChangeProcessor::CreateSyncNode(
    const BookmarkNode* parent,
    BookmarkModel* model,
    int index,
    syncer::WriteTransaction* trans,
    BookmarkModelAssociator* associator,
    syncer::DataTypeErrorHandler* error_handler) {
  const BookmarkNode* child = parent->GetChild(index);
  DCHECK(child);

  // Create a WriteNode container to hold the new node.
  syncer::WriteNode sync_child(trans);

  // Actually create the node with the appropriate initial position.
  if (!PlaceSyncNode(CREATE, parent, index, trans, &sync_child, associator)) {
    syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                            "Failed to create sync node.", syncer::BOOKMARKS);
    error_handler->OnUnrecoverableError(error);
    return syncer::kInvalidId;
  }

  UpdateSyncNodeProperties(child, model, &sync_child, error_handler);

  // Associate the ID from the sync domain with the bookmark node, so that we
  // can refer back to this item later.
  associator->Associate(child, sync_child);

  return sync_child.GetId();
}

void BookmarkChangeProcessor::OnWillRemoveBookmarks(BookmarkModel* model,
                                                    const BookmarkNode* parent,
                                                    int old_index,
                                                    const BookmarkNode* node) {
  if (CanSyncNode(node))
    RemoveSyncNodeHierarchy(node);
}

void BookmarkChangeProcessor::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked) {
  // All the work should have already been done in OnWillRemoveBookmarks.
  DCHECK_EQ(syncer::kInvalidId,
            model_associator_->GetSyncIdFromChromeId(node->id()));
}

void BookmarkChangeProcessor::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  RemoveAllSyncNodes();
}

void BookmarkChangeProcessor::BookmarkNodeChanged(BookmarkModel* model,
                                                  const BookmarkNode* node) {
  if (!CanSyncNode(node))
    return;
  // We shouldn't see changes to the top-level nodes.
  if (model->is_permanent_node(node)) {
    NOTREACHED() << "Saw update to permanent node!";
    return;
  }
  CreateOrUpdateSyncNode(node);
}

// Static.
int64_t BookmarkChangeProcessor::UpdateSyncNode(
    const BookmarkNode* node,
    BookmarkModel* model,
    syncer::WriteTransaction* trans,
    BookmarkModelAssociator* associator,
    syncer::DataTypeErrorHandler* error_handler) {
  // Lookup the sync node that's associated with |node|.
  syncer::WriteNode sync_node(trans);
  if (!associator->InitSyncNodeFromChromeId(node->id(), &sync_node)) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Failed to init sync node from chrome node",
                            syncer::BOOKMARKS);
    error_handler->OnUnrecoverableError(error);
    return syncer::kInvalidId;
  }
  UpdateSyncNodeProperties(node, model, &sync_node, error_handler);
  DCHECK_EQ(sync_node.GetIsFolder(), node->is_folder());
  DCHECK_EQ(associator->GetChromeNodeFromSyncId(sync_node.GetParentId()),
            node->parent());
  DCHECK_EQ(node->parent()->GetIndexOf(node), sync_node.GetPositionIndex());
  return sync_node.GetId();
}

void BookmarkChangeProcessor::BookmarkMetaInfoChanged(
    BookmarkModel* model, const BookmarkNode* node) {
  BookmarkNodeChanged(model, node);
}

void BookmarkChangeProcessor::BookmarkNodeMoved(BookmarkModel* model,
      const BookmarkNode* old_parent, int old_index,
      const BookmarkNode* new_parent, int new_index) {
  const BookmarkNode* child = new_parent->GetChild(new_index);

  if (!CanSyncNode(child))
    return;

  // We shouldn't see changes to the top-level nodes.
  if (model->is_permanent_node(child)) {
    NOTREACHED() << "Saw update to permanent node!";
    return;
  }

  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  {
    // Acquire a scoped write lock via a transaction.
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);

    // Lookup the sync node that's associated with |child|.
    syncer::WriteNode sync_node(&trans);
    if (!model_associator_->InitSyncNodeFromChromeId(child->id(), &sync_node)) {
      syncer::SyncError error(FROM_HERE,
                              syncer::SyncError::DATATYPE_ERROR,
                              "Failed to init sync node from chrome node",
                              syncer::BOOKMARKS);
      error_handler()->OnUnrecoverableError(error);
      return;
    }

    if (!PlaceSyncNode(MOVE, new_parent, new_index, &trans, &sync_node,
                       model_associator_)) {
      syncer::SyncError error(FROM_HERE,
                              syncer::SyncError::DATATYPE_ERROR,
                              "Failed to place sync node",
                              syncer::BOOKMARKS);
      error_handler()->OnUnrecoverableError(error);
      return;
    }
  }

  UpdateTransactionVersion(new_version, model,
                           std::vector<const BookmarkNode*>(1, child));
}

void BookmarkChangeProcessor::BookmarkNodeFaviconChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  if (!CanSyncNode(node)) {
    return;
  }

  // We shouldn't see changes to the top-level nodes.
  if (model->is_permanent_node(node)) {
    NOTREACHED() << "Saw Favicon update to permanent node!";
    return;
  }

  // Ignore favicons that are being loaded.
  if (!node->is_favicon_loaded()) {
    // Sutble way to trigger a load of the favicon.
    model->GetFavicon(node);
    return;
  }

  // Ignore updates to favicon if model associator doesn't know about this
  // bookmark node.
  if (model_associator_->GetSyncIdFromChromeId(node->id()) ==
      syncer::kInvalidId) {
    return;
  }

  CreateOrUpdateSyncNode(node);
}

void BookmarkChangeProcessor::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  if (!CanSyncNode(node))
    return;
  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  std::vector<const BookmarkNode*> children;
  {
    // Acquire a scoped write lock via a transaction.
    syncer::WriteTransaction trans(FROM_HERE, share_handle(), &new_version);

    // The given node's children got reordered. We need to reorder all the
    // children of the corresponding sync node.
    for (int i = 0; i < node->child_count(); ++i) {
      const BookmarkNode* child = node->GetChild(i);
      children.push_back(child);

      syncer::WriteNode sync_child(&trans);
      if (!model_associator_->InitSyncNodeFromChromeId(child->id(),
                                                       &sync_child)) {
        syncer::SyncError error(FROM_HERE,
                                syncer::SyncError::DATATYPE_ERROR,
                                "Failed to init sync node from chrome node",
                                syncer::BOOKMARKS);
        error_handler()->OnUnrecoverableError(error);
        return;
      }
      DCHECK_EQ(sync_child.GetParentId(),
                model_associator_->GetSyncIdFromChromeId(node->id()));

      if (!PlaceSyncNode(MOVE, node, i, &trans, &sync_child,
                         model_associator_)) {
        syncer::SyncError error(FROM_HERE,
                                syncer::SyncError::DATATYPE_ERROR,
                                "Failed to place sync node",
                                syncer::BOOKMARKS);
        error_handler()->OnUnrecoverableError(error);
        return;
      }
    }
  }

  // TODO(haitaol): Filter out children that didn't actually change.
  UpdateTransactionVersion(new_version, model, children);
}

// static
bool BookmarkChangeProcessor::PlaceSyncNode(MoveOrCreate operation,
      const BookmarkNode* parent, int index, syncer::WriteTransaction* trans,
      syncer::WriteNode* dst, BookmarkModelAssociator* associator) {
  syncer::ReadNode sync_parent(trans);
  if (!associator->InitSyncNodeFromChromeId(parent->id(), &sync_parent)) {
    LOG(WARNING) << "Parent lookup failed";
    return false;
  }

  bool success = false;
  if (index == 0) {
    // Insert into first position.
    success = (operation == CREATE)
                  ? dst->InitBookmarkByCreation(sync_parent, nullptr)
                  : dst->SetPosition(sync_parent, nullptr);
    if (success) {
      DCHECK_EQ(dst->GetParentId(), sync_parent.GetId());
      DCHECK_EQ(dst->GetId(), sync_parent.GetFirstChildId());
      DCHECK_EQ(dst->GetPredecessorId(), syncer::kInvalidId);
    }
  } else {
    // Find the bookmark model predecessor, and insert after it.
    const BookmarkNode* prev = parent->GetChild(index - 1);
    syncer::ReadNode sync_prev(trans);
    if (!associator->InitSyncNodeFromChromeId(prev->id(), &sync_prev)) {
      LOG(WARNING) << "Predecessor lookup failed";
      return false;
    }
    success = (operation == CREATE) ?
        dst->InitBookmarkByCreation(sync_parent, &sync_prev) :
        dst->SetPosition(sync_parent, &sync_prev);
    if (success) {
      DCHECK_EQ(dst->GetParentId(), sync_parent.GetId());
      DCHECK_EQ(dst->GetPredecessorId(), sync_prev.GetId());
      DCHECK_EQ(dst->GetId(), sync_prev.GetSuccessorId());
    }
  }
  return success;
}

// ApplyModelChanges is called by the sync backend after changes have been made
// to the sync engine's model.  Apply these changes to the browser bookmark
// model.
void BookmarkChangeProcessor::ApplyChangesFromSyncModel(
    const syncer::BaseTransaction* trans,
    int64_t model_version,
    const syncer::ImmutableChangeRecordList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // A note about ordering.  Sync backend is responsible for ordering the change
  // records in the following order:
  //
  // 1. Deletions, from leaves up to parents.
  // 2. Existing items with synced parents & predecessors.
  // 3. New items with synced parents & predecessors.
  // 4. Items with parents & predecessors in the list.
  // 5. Repeat #4 until all items are in the list.
  //
  // "Predecessor" here means the previous item within a given folder; an item
  // in the first position is always said to have a synced predecessor.
  // For the most part, applying these changes in the order given will yield
  // the correct result.  There is one exception, however: for items that are
  // moved away from a folder that is being deleted, we will process the delete
  // before the move.  Since deletions in the bookmark model propagate from
  // parent to child, we must move them to a temporary location.
  BookmarkModel* model = bookmark_model_;

  // We are going to make changes to the bookmarks model, but don't want to end
  // up in a feedback loop, so remove ourselves as an observer while applying
  // changes.
  model->RemoveObserver(this);

  // Changes made to the bookmark model due to sync should not be undoable.
  ScopedSuspendBookmarkUndo suspend_undo(
      sync_client_->GetBookmarkUndoServiceIfExists());

  // Notify UI intensive observers of BookmarkModel that we are about to make
  // potentially significant changes to it, so the updates may be batched. For
  // example, on Mac, the bookmarks bar displays animations when bookmark items
  // are added or deleted.
  model->BeginExtensiveChanges();

  // A parent to hold nodes temporarily orphaned by parent deletion.  It is
  // created only if it is needed.
  const BookmarkNode* foster_parent = nullptr;

  // Iterate over the deletions, which are always at the front of the list.
  ChangeRecordList::const_iterator it;
  for (it = changes.Get().begin();
       it != changes.Get().end() && it->action == ChangeRecord::ACTION_DELETE;
       ++it) {
    const BookmarkNode* dst =
        model_associator_->GetChromeNodeFromSyncId(it->id);

    // Ignore changes to the permanent top-level nodes.  We only care about
    // their children.
    if (model->is_permanent_node(dst))
      continue;

    // Can't do anything if we can't find the chrome node.
    if (!dst)
      continue;

    // Children of a deleted node should not be deleted; they may be
    // reparented by a later change record.  Move them to a temporary place.
    if (!dst->empty()) {
      if (!foster_parent) {
        foster_parent = model->AddFolder(model->other_node(),
                                         model->other_node()->child_count(),
                                         base::string16());
        if (!foster_parent) {
          syncer::SyncError error(FROM_HERE,
                                  syncer::SyncError::DATATYPE_ERROR,
                                  "Failed to create foster parent",
                                  syncer::BOOKMARKS);
          error_handler()->OnUnrecoverableError(error);
          return;
        }
      }
      for (int i = dst->child_count() - 1; i >= 0; --i) {
        model->Move(dst->GetChild(i), foster_parent,
                    foster_parent->child_count());
      }
    }
    DCHECK_EQ(dst->child_count(), 0) << "Node being deleted has children";

    model_associator_->Disassociate(it->id);

    const BookmarkNode* parent = dst->parent();
    int index = parent->GetIndexOf(dst);
    if (index > -1)
      model->Remove(parent->GetChild(index));
  }

  // A map to keep track of some reordering work we defer until later.
  std::multimap<int, const BookmarkNode*> to_reposition;

  syncer::ReadNode synced_bookmarks(trans);
  int64_t synced_bookmarks_id = syncer::kInvalidId;
  if (synced_bookmarks.InitByTagLookupForBookmarks(kMobileBookmarksTag) ==
      syncer::BaseNode::INIT_OK) {
    synced_bookmarks_id = synced_bookmarks.GetId();
  }

  // Continue iterating where the previous loop left off.
  for ( ; it != changes.Get().end(); ++it) {
    const BookmarkNode* dst =
        model_associator_->GetChromeNodeFromSyncId(it->id);

    // Ignore changes to the permanent top-level nodes.  We only care about
    // their children.
    if (model->is_permanent_node(dst))
      continue;

    // Because the Synced Bookmarks node can be created server side, it's
    // possible it'll arrive at the client as an update. In that case it won't
    // have been associated at startup, the GetChromeNodeFromSyncId call above
    // will return null, and we won't detect it as a permanent node, resulting
    // in us trying to create it here (which will fail). Therefore, we add
    // special logic here just to detect the Synced Bookmarks folder.
    if (synced_bookmarks_id != syncer::kInvalidId &&
        it->id == synced_bookmarks_id) {
      // This is a newly created Synced Bookmarks node. Associate it.
      model_associator_->Associate(model->mobile_node(), synced_bookmarks);
      continue;
    }

    DCHECK_NE(it->action, ChangeRecord::ACTION_DELETE)
        << "We should have passed all deletes by this point.";

    syncer::ReadNode src(trans);
    if (src.InitByIdLookup(it->id) != syncer::BaseNode::INIT_OK) {
      syncer::SyncError error(FROM_HERE,
                              syncer::SyncError::DATATYPE_ERROR,
                              "Failed to load sync node",
                              syncer::BOOKMARKS);
      error_handler()->OnUnrecoverableError(error);
      return;
    }

    const BookmarkNode* parent =
        model_associator_->GetChromeNodeFromSyncId(src.GetParentId());
    if (!parent) {
      LOG(ERROR) << "Could not find parent of node being added/updated."
        << " Node title: " << src.GetTitle()
        << ", parent id = " << src.GetParentId();
      continue;
    }

    if (dst) {
      DCHECK(it->action == ChangeRecord::ACTION_UPDATE)
          << "ACTION_UPDATE should be seen if and only if the node is known.";
      UpdateBookmarkWithSyncData(src, model, dst, sync_client_);

      // Move all modified entries to the right.  We'll fix it later.
      model->Move(dst, parent, parent->child_count());
    } else {
      DCHECK(it->action == ChangeRecord::ACTION_ADD)
          << "ACTION_ADD should be seen if and only if the node is unknown.";

      dst = CreateBookmarkNode(&src, parent, model, sync_client_,
                               parent->child_count());
      if (!dst) {
        // We ignore bookmarks we can't add. Chances are this is caused by
        // a bookmark that was not fully associated.
        LOG(ERROR) << "Failed to create bookmark node with title "
                   << src.GetTitle() + " and url "
                   << src.GetBookmarkSpecifics().url();
        continue;
      }
      model_associator_->Associate(dst, src);
    }

    to_reposition.insert(std::make_pair(src.GetPositionIndex(), dst));
    bookmark_model_->SetNodeSyncTransactionVersion(dst, model_version);
  }

  // When we added or updated bookmarks in the previous loop, we placed them to
  // the far right position.  Now we iterate over all these modified items in
  // sync order, left to right, moving them into their proper positions.
  for (auto it = to_reposition.begin(); it != to_reposition.end(); ++it) {
    const BookmarkNode* parent = it->second->parent();
    model->Move(it->second, parent, it->first);
  }

  // Clean up the temporary node.
  if (foster_parent) {
    // There should be no nodes left under the foster parent.
    DCHECK_EQ(foster_parent->child_count(), 0);
    model->Remove(foster_parent);
    foster_parent = nullptr;
  }

  // Notify UI intensive observers of BookmarkModel that all updates have been
  // applied, and that they may now be consumed. This prevents issues like the
  // one described in crbug.com/281562, where old and new items on the bookmarks
  // bar would overlap.
  model->EndExtensiveChanges();

  // We are now ready to hear about bookmarks changes again.
  model->AddObserver(this);

  // All changes are applied in bookmark model. Set transaction version on
  // bookmark model to mark as synced.
  model->SetNodeSyncTransactionVersion(model->root_node(), model_version);
}

// Static.
// Update a bookmark node with specified sync data.
void BookmarkChangeProcessor::UpdateBookmarkWithSyncData(
    const syncer::BaseNode& sync_node,
    BookmarkModel* model,
    const BookmarkNode* node,
    syncer::SyncClient* sync_client) {
  DCHECK_EQ(sync_node.GetIsFolder(), node->is_folder());
  const sync_pb::BookmarkSpecifics& specifics =
      sync_node.GetBookmarkSpecifics();
  if (!sync_node.GetIsFolder())
    model->SetURL(node, GURL(specifics.url()));
  model->SetTitle(node, base::UTF8ToUTF16(sync_node.GetTitle()));
  if (specifics.has_creation_time_us()) {
    model->SetDateAdded(
        node,
        base::Time::FromInternalValue(specifics.creation_time_us()));
  }
  SetBookmarkFavicon(&sync_node, node, sync_client);
  model->SetNodeMetaInfoMap(node, *GetBookmarkMetaInfo(&sync_node));
}

// static
void BookmarkChangeProcessor::UpdateTransactionVersion(
    int64_t new_version,
    BookmarkModel* model,
    const std::vector<const BookmarkNode*>& nodes) {
  if (new_version != syncer::syncable::kInvalidTransactionVersion) {
    model->SetNodeSyncTransactionVersion(model->root_node(), new_version);
    for (size_t i = 0; i < nodes.size(); ++i) {
      model->SetNodeSyncTransactionVersion(nodes[i], new_version);
    }
  }
}

// static
// Creates a bookmark node under the given parent node from the given sync
// node. Returns the newly created node.
const BookmarkNode* BookmarkChangeProcessor::CreateBookmarkNode(
    const syncer::BaseNode* sync_node,
    const BookmarkNode* parent,
    BookmarkModel* model,
    syncer::SyncClient* sync_client,
    int index) {
  return CreateBookmarkNode(base::UTF8ToUTF16(sync_node->GetTitle()),
                            GURL(sync_node->GetBookmarkSpecifics().url()),
                            sync_node, parent, model, sync_client, index);
}

// static
// Creates a bookmark node under the given parent node from the given sync
// node. Returns the newly created node.
const BookmarkNode* BookmarkChangeProcessor::CreateBookmarkNode(
    const base::string16& title,
    const GURL& url,
    const syncer::BaseNode* sync_node,
    const BookmarkNode* parent,
    BookmarkModel* model,
    syncer::SyncClient* sync_client,
    int index) {
  DCHECK(parent);

  const BookmarkNode* node;
  if (sync_node->GetIsFolder()) {
    node = model->AddFolderWithMetaInfo(parent, index, title,
                                        GetBookmarkMetaInfo(sync_node).get());
  } else {
    // 'creation_time_us' was added in m24. Assume a time of 0 means now.
    const sync_pb::BookmarkSpecifics& specifics =
        sync_node->GetBookmarkSpecifics();
    const int64_t create_time_internal = specifics.creation_time_us();
    base::Time create_time = (create_time_internal == 0) ?
        base::Time::Now() : base::Time::FromInternalValue(create_time_internal);
    node = model->AddURLWithCreationTimeAndMetaInfo(
        parent, index, title, url, create_time,
        GetBookmarkMetaInfo(sync_node).get());
    if (node)
      SetBookmarkFavicon(sync_node, node, sync_client);
  }

  return node;
}

// static
// Sets the favicon of the given bookmark node from the given sync node.
void BookmarkChangeProcessor::SetBookmarkFavicon(
    const syncer::BaseNode* sync_node,
    const BookmarkNode* bookmark_node,
    syncer::SyncClient* sync_client) {
  const sync_pb::BookmarkSpecifics& specifics =
      sync_node->GetBookmarkSpecifics();
  const std::string& icon_bytes_str = specifics.favicon();
  scoped_refptr<base::RefCountedString> icon_bytes(
      new base::RefCountedString());
  icon_bytes->data().assign(icon_bytes_str);

  ApplyBookmarkFavicon(bookmark_node, sync_client, GURL(specifics.icon_url()),
                       icon_bytes);
}

// static
std::unique_ptr<BookmarkNode::MetaInfoMap>
BookmarkChangeProcessor::GetBookmarkMetaInfo(
    const syncer::BaseNode* sync_node) {
  const sync_pb::BookmarkSpecifics& specifics =
      sync_node->GetBookmarkSpecifics();
  std::unique_ptr<BookmarkNode::MetaInfoMap> meta_info_map(
      new BookmarkNode::MetaInfoMap);
  for (int i = 0; i < specifics.meta_info_size(); ++i) {
    (*meta_info_map)[specifics.meta_info(i).key()] =
        specifics.meta_info(i).value();
  }
  // Verifies that all entries had unique keys.
  DCHECK_EQ(static_cast<size_t>(specifics.meta_info_size()),
            meta_info_map->size());
  return meta_info_map;
}

// static
void BookmarkChangeProcessor::SetSyncNodeMetaInfo(
    const BookmarkNode* node,
    syncer::WriteNode* sync_node) {
  sync_pb::BookmarkSpecifics specifics = sync_node->GetBookmarkSpecifics();
  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();

  // Compare specifics meta info to node meta info before making the change.
  // Please note that the original specifics meta info is unordered while
  //  meta_info_map is ordered by key. Setting the meta info blindly into
  // the specifics might cause an unnecessary change.
  size_t size = meta_info_map ? meta_info_map->size() : 0;
  if (static_cast<size_t>(specifics.meta_info_size()) == size) {
    size_t index = 0;
    for (; index < size; index++) {
      const sync_pb::MetaInfo& meta_info = specifics.meta_info(index);
      auto it = meta_info_map->find(meta_info.key());
      if (it == meta_info_map->end() || it->second != meta_info.value()) {
        // One of original meta info entries is missing in |meta_info_map| or
        // different.
        break;
      }
    }
    if (index == size) {
      // The original meta info from the sync model is already equivalent to
      // |meta_info_map|.
      return;
    }
  }

  // Clear and reset meta info in bookmark specifics.
  specifics.clear_meta_info();
  if (meta_info_map) {
    for (auto it = meta_info_map->begin(); it != meta_info_map->end(); ++it) {
      sync_pb::MetaInfo* meta_info = specifics.add_meta_info();
      meta_info->set_key(it->first);
      meta_info->set_value(it->second);
    }
  }

  sync_node->SetBookmarkSpecifics(specifics);
}

// static
void BookmarkChangeProcessor::ApplyBookmarkFavicon(
    const BookmarkNode* bookmark_node,
    syncer::SyncClient* sync_client,
    const GURL& icon_url,
    const scoped_refptr<base::RefCountedMemory>& bitmap_data) {
  favicon::FaviconService* favicon_service = sync_client->GetFaviconService();

  // Some tests (that use FakeSyncClient) use no services.
  if (favicon_service == nullptr)
    return;

  favicon_service->AddPageNoVisitForBookmark(bookmark_node->url(),
                                             bookmark_node->GetTitle());

  GURL icon_url_to_use = icon_url;

  if (icon_url.is_empty()) {
    if (bitmap_data->size() == 0) {
      // Empty icon URL and no bitmap data means no icon mapping.
      favicon_service->DeleteFaviconMappings({bookmark_node->url()},
                                             favicon_base::IconType::kFavicon);
      return;
    } else {
      // Ancient clients (prior to M25) may not be syncing the favicon URL. If
      // the icon URL is not synced, use the page URL as a fake icon URL as it
      // is guaranteed to be unique.
      icon_url_to_use = bookmark_node->url();
    }
  }

  // The client may have cached the favicon at 2x. Use MergeFavicon() as not to
  // overwrite the cached 2x favicon bitmap. Sync favicons are always
  // gfx::kFaviconSize in width and height. Store the favicon into history
  // as such.
  gfx::Size pixel_size(gfx::kFaviconSize, gfx::kFaviconSize);
  favicon_service->MergeFavicon(bookmark_node->url(), icon_url_to_use,
                                favicon_base::IconType::kFavicon, bitmap_data,
                                pixel_size);
}

// static
void BookmarkChangeProcessor::SetSyncNodeFavicon(
    const BookmarkNode* bookmark_node,
    BookmarkModel* model,
    syncer::WriteNode* sync_node) {
  scoped_refptr<base::RefCountedMemory> favicon_bytes(nullptr);
  EncodeFavicon(bookmark_node, model, &favicon_bytes);
  sync_pb::BookmarkSpecifics updated_specifics(
      sync_node->GetBookmarkSpecifics());

  if (favicon_bytes.get() && favicon_bytes->size()) {
    updated_specifics.set_favicon(favicon_bytes->front(),
                                  favicon_bytes->size());
    updated_specifics.set_icon_url(bookmark_node->icon_url()
                                       ? bookmark_node->icon_url()->spec()
                                       : std::string());
  } else {
    updated_specifics.clear_favicon();
    updated_specifics.clear_icon_url();
  }

  sync_node->SetBookmarkSpecifics(updated_specifics);
}

bool BookmarkChangeProcessor::CanSyncNode(const BookmarkNode* node) {
  return bookmark_model_->client()->CanSyncNode(node);
}

}  // namespace sync_bookmarks
