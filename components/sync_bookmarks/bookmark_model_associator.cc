// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_associator.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sync/base/cryptographer.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/syncable/delete_journal.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"
#include "components/sync_bookmarks/bookmark_change_processor.h"
#include "components/undo/bookmark_undo_service.h"
#include "components/undo/bookmark_undo_utils.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace sync_bookmarks {

// The sync protocol identifies top-level entities by means of well-known tags,
// which should not be confused with titles.  Each tag corresponds to a
// singleton instance of a particular top-level node in a user's share; the
// tags are consistent across users. The tags allow us to locate the specific
// folders whose contents we care about synchronizing, without having to do a
// lookup by name or path.  The tags should not be made user-visible.
// For example, the tag "bookmark_bar" represents the permanent node for
// bookmarks bar in Chrome. The tag "other_bookmarks" represents the permanent
// folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing,
// the sync server) to create these tagged nodes when initializing sync
// for the first time for a user.  Thus, once the backend finishes
// initializing, the ProfileSyncService can rely on the presence of tagged
// nodes.
//
// TODO(ncarter): Pull these tags from an external protocol specification
// rather than hardcoding them here.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Maximum number of bytes to allow in a title (must match sync's internal
// limits; see write_node.cc).
const int kTitleLimitBytes = 255;

// Provides the following abstraction: given a parent bookmark node, find best
// matching child node for many sync nodes.
class BookmarkNodeFinder {
 public:
  // Creates an instance with the given parent bookmark node.
  explicit BookmarkNodeFinder(const BookmarkNode* parent_node);

  // Finds the bookmark node that matches the given url, title and folder
  // attribute. Returns the matching node if one exists; null otherwise.
  // If there are multiple matches then a node with ID matching |preferred_id|
  // is returned; otherwise the first matching node is returned.
  // If a matching node is found, it's removed for further matches.
  const BookmarkNode* FindBookmarkNode(const GURL& url,
                                       const std::string& title,
                                       bool is_folder,
                                       int64_t preferred_id);

  // Returns true if |bookmark_node| matches the specified |url|,
  // |title|, and |is_folder| flags.
  static bool NodeMatches(const BookmarkNode* bookmark_node,
                          const GURL& url,
                          const std::string& title,
                          bool is_folder);

 private:
  // Maps bookmark node titles to instances, duplicates allowed.
  // Titles are converted to the sync internal format before
  // being used as keys for the map.
  using BookmarkNodeMap = std::multimap<std::string, const BookmarkNode*>;
  using BookmarkNodeRange =
      std::pair<BookmarkNodeMap::iterator, BookmarkNodeMap::iterator>;

  // Converts and truncates bookmark titles in the form sync does internally
  // to avoid mismatches due to sync munging titles.
  static void ConvertTitleToSyncInternalFormat(const std::string& input,
                                               std::string* output);

  const BookmarkNode* parent_node_;
  BookmarkNodeMap child_nodes_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeFinder);
};

class ScopedAssociationUpdater {
 public:
  explicit ScopedAssociationUpdater(BookmarkModel* model) {
    model_ = model;
    model->BeginExtensiveChanges();
  }

  ~ScopedAssociationUpdater() {
    model_->EndExtensiveChanges();
  }

 private:
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAssociationUpdater);
};

BookmarkNodeFinder::BookmarkNodeFinder(const BookmarkNode* parent_node)
    : parent_node_(parent_node) {
  for (int i = 0; i < parent_node_->child_count(); ++i) {
    const BookmarkNode* child_node = parent_node_->GetChild(i);

    std::string title = base::UTF16ToUTF8(child_node->GetTitle());
    ConvertTitleToSyncInternalFormat(title, &title);

    child_nodes_.insert(std::make_pair(title, child_node));
  }
}

const BookmarkNode* BookmarkNodeFinder::FindBookmarkNode(
    const GURL& url,
    const std::string& title,
    bool is_folder,
    int64_t preferred_id) {
  const BookmarkNode* match = nullptr;

  // First lookup a range of bookmarks with the same title.
  std::string adjusted_title;
  ConvertTitleToSyncInternalFormat(title, &adjusted_title);
  BookmarkNodeRange range = child_nodes_.equal_range(adjusted_title);
  auto match_iter = range.second;
  for (auto iter = range.first; iter != range.second; ++iter) {
    // Then within the range match the node by the folder bit
    // and the url.
    const BookmarkNode* node = iter->second;
    if (is_folder == node->is_folder() && url == node->url()) {
      if (node->id() == preferred_id || preferred_id == 0) {
        // Preferred match - use this node.
        match = node;
        match_iter = iter;
        break;
      } else if (match == nullptr) {
        // First match - continue iterating.
        match = node;
        match_iter = iter;
      }
    }
  }

  if (match_iter != range.second) {
    // Remove the matched node so we don't match with it again.
    child_nodes_.erase(match_iter);
  }

  return match;
}

/* static */
bool BookmarkNodeFinder::NodeMatches(const BookmarkNode* bookmark_node,
                                     const GURL& url,
                                     const std::string& title,
                                     bool is_folder) {
  if (url != bookmark_node->url() || is_folder != bookmark_node->is_folder()) {
    return false;
  }

  // The title passed to this method comes from a sync directory entry.
  // The following two lines are needed to make the native bookmark title
  // comparable. The same conversion is used in BookmarkNodeFinder constructor.
  std::string bookmark_title = base::UTF16ToUTF8(bookmark_node->GetTitle());
  ConvertTitleToSyncInternalFormat(bookmark_title, &bookmark_title);
  return title == bookmark_title;
}

/* static */
void BookmarkNodeFinder::ConvertTitleToSyncInternalFormat(
    const std::string& input, std::string* output) {
  syncer::SyncAPINameToServerName(input, output);
  base::TruncateUTF8ToByteSize(*output, kTitleLimitBytes, output);
}

BookmarkModelAssociator::Context::Context(
    syncer::SyncMergeResult* local_merge_result,
    syncer::SyncMergeResult* syncer_merge_result)
    : local_merge_result_(local_merge_result),
      syncer_merge_result_(syncer_merge_result),
      duplicate_count_(0),
      native_model_sync_state_(UNSET) {}

BookmarkModelAssociator::Context::~Context() {
}

void BookmarkModelAssociator::Context::PushNode(int64_t sync_id) {
  dfs_stack_.push(sync_id);
}

bool BookmarkModelAssociator::Context::PopNode(int64_t* sync_id) {
  if (dfs_stack_.empty()) {
    *sync_id = 0;
    return false;
  }
  *sync_id = dfs_stack_.top();
  dfs_stack_.pop();
  return true;
}

void BookmarkModelAssociator::Context::SetPreAssociationVersions(
    int64_t native_version,
    int64_t sync_version) {
  local_merge_result_->set_pre_association_version(native_version);
  syncer_merge_result_->set_pre_association_version(sync_version);
}

void BookmarkModelAssociator::Context::SetNumItemsBeforeAssociation(
    int local_num,
    int sync_num) {
  local_merge_result_->set_num_items_before_association(local_num);
  syncer_merge_result_->set_num_items_before_association(sync_num);
}

void BookmarkModelAssociator::Context::SetNumItemsAfterAssociation(
    int local_num,
    int sync_num) {
  local_merge_result_->set_num_items_after_association(local_num);
  syncer_merge_result_->set_num_items_after_association(sync_num);
}

void BookmarkModelAssociator::Context::IncrementLocalItemsDeleted() {
  local_merge_result_->set_num_items_deleted(
      local_merge_result_->num_items_deleted() + 1);
}

void BookmarkModelAssociator::Context::IncrementLocalItemsAdded() {
  local_merge_result_->set_num_items_added(
      local_merge_result_->num_items_added() + 1);
}

void BookmarkModelAssociator::Context::IncrementLocalItemsModified() {
  local_merge_result_->set_num_items_modified(
      local_merge_result_->num_items_modified() + 1);
}

void BookmarkModelAssociator::Context::IncrementSyncItemsAdded() {
  syncer_merge_result_->set_num_items_added(
      syncer_merge_result_->num_items_added() + 1);
}

void BookmarkModelAssociator::Context::IncrementSyncItemsDeleted(int count) {
  syncer_merge_result_->set_num_items_deleted(
      syncer_merge_result_->num_items_deleted() + count);
}

void BookmarkModelAssociator::Context::UpdateDuplicateCount(
    const base::string16& title,
    const GURL& url) {
  size_t bookmark_hash = base::Hash(title) ^ base::Hash(url.spec());
  if (!hashes_.insert(bookmark_hash).second) {
    // This hash code already exists in the set.
    ++duplicate_count_;
  }
}

void BookmarkModelAssociator::Context::AddBookmarkRoot(
    const bookmarks::BookmarkNode* root) {
  bookmark_roots_.push_back(root);
}

int64_t BookmarkModelAssociator::Context::GetSyncPreAssociationVersion() const {
  return syncer_merge_result_->pre_association_version();
}

void BookmarkModelAssociator::Context::MarkForVersionUpdate(
    const bookmarks::BookmarkNode* node) {
  bookmarks_for_version_update_.push_back(node);
}

BookmarkModelAssociator::BookmarkModelAssociator(
    BookmarkModel* bookmark_model,
    syncer::SyncClient* sync_client,
    syncer::UserShare* user_share,
    std::unique_ptr<syncer::DataTypeErrorHandler> unrecoverable_error_handler,
    bool expect_mobile_bookmarks_folder)
    : bookmark_model_(bookmark_model),
      sync_client_(sync_client),
      user_share_(user_share),
      unrecoverable_error_handler_(std::move(unrecoverable_error_handler)),
      expect_mobile_bookmarks_folder_(expect_mobile_bookmarks_folder),
      weak_factory_(this) {
  DCHECK(bookmark_model_);
  DCHECK(user_share_);
  DCHECK(unrecoverable_error_handler_);
}

BookmarkModelAssociator::~BookmarkModelAssociator() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

syncer::SyncError BookmarkModelAssociator::DisassociateModels() {
  id_map_.clear();
  id_map_inverse_.clear();
  dirty_associations_sync_ids_.clear();
  return syncer::SyncError();
}

int64_t BookmarkModelAssociator::GetSyncIdFromChromeId(const int64_t& node_id) {
  BookmarkIdToSyncIdMap::const_iterator iter = id_map_.find(node_id);
  return iter == id_map_.end() ? syncer::kInvalidId : iter->second;
}

const BookmarkNode* BookmarkModelAssociator::GetChromeNodeFromSyncId(
    int64_t sync_id) {
  SyncIdToBookmarkNodeMap::const_iterator iter = id_map_inverse_.find(sync_id);
  return iter == id_map_inverse_.end() ? nullptr : iter->second;
}

bool BookmarkModelAssociator::InitSyncNodeFromChromeId(
    const int64_t& node_id,
    syncer::BaseNode* sync_node) {
  DCHECK(sync_node);
  int64_t sync_id = GetSyncIdFromChromeId(node_id);
  if (sync_id == syncer::kInvalidId)
    return false;
  if (sync_node->InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK)
    return false;
  DCHECK(sync_node->GetId() == sync_id);
  return true;
}

void BookmarkModelAssociator::AddAssociation(const BookmarkNode* node,
                                             int64_t sync_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64_t node_id = node->id();
  DCHECK_NE(sync_id, syncer::kInvalidId);
  DCHECK(id_map_.find(node_id) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[node_id] = sync_id;
  id_map_inverse_[sync_id] = node;
}

void BookmarkModelAssociator::Associate(const BookmarkNode* node,
                                        const syncer::BaseNode& sync_node) {
  AddAssociation(node, sync_node.GetId());

  // The same check exists in PersistAssociations. However it is better to
  // do the check earlier to avoid the cost of decrypting nodes again
  // in PersistAssociations.
  if (node->id() != sync_node.GetExternalId()) {
    dirty_associations_sync_ids_.insert(sync_node.GetId());
    PostPersistAssociationsTask();
  }
}

void BookmarkModelAssociator::Disassociate(int64_t sync_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  id_map_.erase(iter->second->id());
  id_map_inverse_.erase(iter);
  dirty_associations_sync_ids_.erase(sync_id);
}

bool BookmarkModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  bool has_mobile_folder = true;

  syncer::ReadTransaction trans(FROM_HERE, user_share_);

  syncer::ReadNode bookmark_bar_node(&trans);
  if (bookmark_bar_node.InitByTagLookupForBookmarks(kBookmarkBarTag) !=
      syncer::BaseNode::INIT_OK) {
    return false;
  }

  syncer::ReadNode other_bookmarks_node(&trans);
  if (other_bookmarks_node.InitByTagLookupForBookmarks(kOtherBookmarksTag) !=
      syncer::BaseNode::INIT_OK) {
    return false;
  }

  syncer::ReadNode mobile_bookmarks_node(&trans);
  if (mobile_bookmarks_node.InitByTagLookupForBookmarks(kMobileBookmarksTag) !=
      syncer::BaseNode::INIT_OK) {
    has_mobile_folder = false;
  }

  // Sync model has user created nodes if any of the permanent nodes has
  // children.
  *has_nodes = bookmark_bar_node.HasChildren() ||
      other_bookmarks_node.HasChildren() ||
      (has_mobile_folder && mobile_bookmarks_node.HasChildren());
  return true;
}

bool BookmarkModelAssociator::AssociateTaggedPermanentNode(
    syncer::BaseTransaction* trans,
    const BookmarkNode* permanent_node,
    const std::string& tag) {
  // Do nothing if |permanent_node| is already initialized and associated.
  int64_t sync_id = GetSyncIdFromChromeId(permanent_node->id());
  if (sync_id != syncer::kInvalidId)
    return true;

  syncer::ReadNode sync_node(trans);
  if (sync_node.InitByTagLookupForBookmarks(tag) != syncer::BaseNode::INIT_OK)
    return false;

  Associate(permanent_node, sync_node);
  return true;
}

syncer::SyncError BookmarkModelAssociator::AssociateModels(
    syncer::SyncMergeResult* local_merge_result,
    syncer::SyncMergeResult* syncer_merge_result) {
  // Since any changes to the bookmark model made here are not user initiated,
  // these change should not be undoable and so suspend the undo tracking.
  ScopedSuspendBookmarkUndo suspend_undo(
      sync_client_->GetBookmarkUndoServiceIfExists());

  Context context(local_merge_result, syncer_merge_result);

  syncer::SyncError error = CheckModelSyncState(&context);
  if (error.IsSet())
    return error;

  std::unique_ptr<ScopedAssociationUpdater> association_updater(
      new ScopedAssociationUpdater(bookmark_model_));
  DisassociateModels();

  error = BuildAssociations(&context);
  if (error.IsSet()) {
    // Clear version on bookmark model so that the conservative association
    // algorithm is used on the next association.
    bookmark_model_->SetNodeSyncTransactionVersion(
        bookmark_model_->root_node(),
        syncer::syncable::kInvalidTransactionVersion);
  }

  return error;
}

syncer::SyncError BookmarkModelAssociator::AssociatePermanentFolders(
    syncer::BaseTransaction* trans,
    Context* context) {
  // To prime our association, we associate the top-level nodes, Bookmark Bar
  // and Other Bookmarks.
  if (!AssociateTaggedPermanentNode(trans, bookmark_model_->bookmark_bar_node(),
                                    kBookmarkBarTag)) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Bookmark bar node not found", model_type());
  }

  if (!AssociateTaggedPermanentNode(trans, bookmark_model_->other_node(),
                                    kOtherBookmarksTag)) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Other bookmarks node not found", model_type());
  }

  if (!AssociateTaggedPermanentNode(trans, bookmark_model_->mobile_node(),
                                    kMobileBookmarksTag) &&
      expect_mobile_bookmarks_folder_) {
    return unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Mobile bookmarks node not found", model_type());
  }

  // Note: the root node may have additional extra nodes. Currently none of
  // them are meant to sync.
  int64_t bookmark_bar_sync_id =
      GetSyncIdFromChromeId(bookmark_model_->bookmark_bar_node()->id());
  DCHECK_NE(bookmark_bar_sync_id, syncer::kInvalidId);
  context->AddBookmarkRoot(bookmark_model_->bookmark_bar_node());
  int64_t other_bookmarks_sync_id =
      GetSyncIdFromChromeId(bookmark_model_->other_node()->id());
  DCHECK_NE(other_bookmarks_sync_id, syncer::kInvalidId);
  context->AddBookmarkRoot(bookmark_model_->other_node());
  int64_t mobile_bookmarks_sync_id =
      GetSyncIdFromChromeId(bookmark_model_->mobile_node()->id());
  if (expect_mobile_bookmarks_folder_)
    DCHECK_NE(syncer::kInvalidId, mobile_bookmarks_sync_id);
  if (mobile_bookmarks_sync_id != syncer::kInvalidId)
    context->AddBookmarkRoot(bookmark_model_->mobile_node());

  // WARNING: The order in which we push these should match their order in the
  // bookmark model (see BookmarkModel::DoneLoading(..)).
  context->PushNode(bookmark_bar_sync_id);
  context->PushNode(other_bookmarks_sync_id);
  if (mobile_bookmarks_sync_id != syncer::kInvalidId)
    context->PushNode(mobile_bookmarks_sync_id);

  return syncer::SyncError();
}

void BookmarkModelAssociator::SetNumItemsBeforeAssociation(
    syncer::BaseTransaction* trans,
    Context* context) {
  int syncer_num = 0;
  syncer::ReadNode bm_root(trans);
  if (bm_root.InitTypeRoot(syncer::BOOKMARKS) == syncer::BaseNode::INIT_OK) {
    syncer_num = bm_root.GetTotalNodeCount();
  }
  context->SetNumItemsBeforeAssociation(
      GetTotalBookmarkCountAndRecordDuplicates(bookmark_model_->root_node(),
                                               context),
      syncer_num);
}

int BookmarkModelAssociator::GetTotalBookmarkCountAndRecordDuplicates(
    const bookmarks::BookmarkNode* node,
    Context* context) const {
  int count = 1;  // Start with one to include the node itself.

  if (!node->is_root()) {
    context->UpdateDuplicateCount(node->GetTitle(), node->url());
  }

  for (int i = 0; i < node->child_count(); ++i) {
    count +=
        GetTotalBookmarkCountAndRecordDuplicates(node->GetChild(i), context);
  }

  return count;
}

void BookmarkModelAssociator::SetNumItemsAfterAssociation(
    syncer::BaseTransaction* trans,
    Context* context) {
  int syncer_num = 0;
  syncer::ReadNode bm_root(trans);
  if (bm_root.InitTypeRoot(syncer::BOOKMARKS) == syncer::BaseNode::INIT_OK) {
    syncer_num = bm_root.GetTotalNodeCount();
  }
  context->SetNumItemsAfterAssociation(
      bookmark_model_->root_node()->GetTotalNodeCount(), syncer_num);
}

syncer::SyncError BookmarkModelAssociator::BuildAssociations(Context* context) {
  DCHECK(bookmark_model_->loaded());
  DCHECK_NE(context->native_model_sync_state(), AHEAD);

  int initial_duplicate_count = 0;
  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  {
    syncer::WriteTransaction trans(FROM_HERE, user_share_, &new_version);

    syncer::SyncError error = AssociatePermanentFolders(&trans, context);
    if (error.IsSet())
      return error;

    SetNumItemsBeforeAssociation(&trans, context);
    initial_duplicate_count = context->duplicate_count();

    // Remove obsolete bookmarks according to sync delete journal.
    // TODO(stanisc): crbug.com/456876: rewrite this to avoid a separate
    // traversal and instead perform deletes at the end of the loop below where
    // the unmatched bookmark nodes are created as sync nodes.
    ApplyDeletesFromSyncJournal(&trans, context);

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
    // selects the preferred one based on sync node external ID match to the
    // bookmark folder ID.
    int64_t sync_parent_id;
    while (context->PopNode(&sync_parent_id)) {
      syncer::ReadNode sync_parent(&trans);
      if (sync_parent.InitByIdLookup(sync_parent_id) !=
          syncer::BaseNode::INIT_OK) {
        return unrecoverable_error_handler_->CreateAndUploadError(
            FROM_HERE, "Failed to lookup node.", model_type());
      }
      // Only folder nodes are pushed on to the stack.
      DCHECK(sync_parent.GetIsFolder());

      const BookmarkNode* parent_node = GetChromeNodeFromSyncId(sync_parent_id);
      if (!parent_node) {
        return unrecoverable_error_handler_->CreateAndUploadError(
            FROM_HERE, "Failed to find bookmark node for sync id.",
            model_type());
      }
      DCHECK(parent_node->is_folder());

      std::vector<int64_t> children;
      sync_parent.GetChildIds(&children);

      error = BuildAssociations(&trans, parent_node, children, context);
      if (error.IsSet())
        return error;
    }

    SetNumItemsAfterAssociation(&trans, context);
  }

  if (new_version == syncer::syncable::kInvalidTransactionVersion) {
    // If we get here it means that none of Sync nodes were modified by the
    // association process.
    // We need to set |new_version| to the pre-association Sync version;
    // otherwise BookmarkChangeProcessor::UpdateTransactionVersion call below
    // won't save it to the native model. That is necessary to ensure that the
    // native model doesn't get stuck at "unset" version and skips any further
    // version checks.
    new_version = context->GetSyncPreAssociationVersion();
  }

  BookmarkChangeProcessor::UpdateTransactionVersion(
      new_version, bookmark_model_, context->bookmarks_for_version_update());

  UMA_HISTOGRAM_COUNTS_1M("Sync.BookmarksDuplicationsAtAssociation",
                          context->duplicate_count());
  UMA_HISTOGRAM_COUNTS_1M("Sync.BookmarksNewDuplicationsAtAssociation",
                          context->duplicate_count() - initial_duplicate_count);

  if (context->duplicate_count() > initial_duplicate_count) {
    UMA_HISTOGRAM_ENUMERATION("Sync.BookmarksModelSyncStateAtNewDuplication",
                              context->native_model_sync_state(),
                              NATIVE_MODEL_SYNC_STATE_COUNT);
  }

  return syncer::SyncError();
}

syncer::SyncError BookmarkModelAssociator::BuildAssociations(
    syncer::WriteTransaction* trans,
    const BookmarkNode* parent_node,
    const std::vector<int64_t>& sync_ids,
    Context* context) {
  BookmarkNodeFinder node_finder(parent_node);

  int index = 0;
  for (auto it = sync_ids.begin(); it != sync_ids.end(); ++it) {
    int64_t sync_child_id = *it;
    syncer::ReadNode sync_child_node(trans);
    if (sync_child_node.InitByIdLookup(sync_child_id) !=
        syncer::BaseNode::INIT_OK) {
      return unrecoverable_error_handler_->CreateAndUploadError(
          FROM_HERE, "Failed to lookup node.", model_type());
    }

    int64_t external_id = sync_child_node.GetExternalId();
    GURL url(sync_child_node.GetBookmarkSpecifics().url());
    const BookmarkNode* child_node = node_finder.FindBookmarkNode(
        url, sync_child_node.GetTitle(), sync_child_node.GetIsFolder(),
        external_id);
    if (child_node) {
      // Skip local node update if the local model version matches and
      // the node is already associated and in the right position.
      bool is_in_sync = (context->native_model_sync_state() == IN_SYNC) &&
                        (child_node->id() == external_id) &&
                        (index < parent_node->child_count()) &&
                        (parent_node->GetChild(index) == child_node);
      if (!is_in_sync) {
        BookmarkChangeProcessor::UpdateBookmarkWithSyncData(
            sync_child_node, bookmark_model_, child_node, sync_client_);
        bookmark_model_->Move(child_node, parent_node, index);
        context->IncrementLocalItemsModified();
        context->MarkForVersionUpdate(child_node);
      }
    } else {
      syncer::SyncError error;
      child_node = CreateBookmarkNode(parent_node, index, &sync_child_node, url,
                                      context, &error);
      if (!child_node) {
        if (error.IsSet()) {
          return error;
        } else {
          // Skip this node and continue. Don't increment index in this case.
          continue;
        }
      }
      context->IncrementLocalItemsAdded();
      context->MarkForVersionUpdate(child_node);
    }

    Associate(child_node, sync_child_node);

    if (sync_child_node.GetIsFolder())
      context->PushNode(sync_child_id);
    ++index;
  }

  // At this point all the children nodes of the parent sync node have
  // corresponding children in the parent bookmark node and they are all in
  // the right positions: from 0 to index - 1.
  // So the children starting from index in the parent bookmark node are the
  // ones that are not present in the parent sync node. So create them.
  for (int i = index; i < parent_node->child_count(); ++i) {
    int64_t sync_child_id = BookmarkChangeProcessor::CreateSyncNode(
        parent_node, bookmark_model_, i, trans, this,
        unrecoverable_error_handler_.get());
    if (syncer::kInvalidId == sync_child_id) {
      return unrecoverable_error_handler_->CreateAndUploadError(
          FROM_HERE, "Failed to create sync node.", model_type());
    }

    context->IncrementSyncItemsAdded();
    const BookmarkNode* child_node = parent_node->GetChild(i);
    context->MarkForVersionUpdate(child_node);
    if (child_node->is_folder())
      context->PushNode(sync_child_id);
  }

  return syncer::SyncError();
}

const BookmarkNode* BookmarkModelAssociator::CreateBookmarkNode(
    const BookmarkNode* parent_node,
    int bookmark_index,
    const syncer::BaseNode* sync_child_node,
    const GURL& url,
    Context* context,
    syncer::SyncError* error) {
  DCHECK_LE(bookmark_index, parent_node->child_count());

  const std::string& sync_title = sync_child_node->GetTitle();

  if (!sync_child_node->GetIsFolder() && !url.is_valid()) {
    LOG(WARNING) << "Cannot associate sync node "
                 << sync_child_node->GetSyncId().value() << " with invalid url "
                 << url.possibly_invalid_spec() << " and title " << sync_title;
    // Don't propagate an error to the model_type in this case.
    return nullptr;
  }

  base::string16 bookmark_title = base::UTF8ToUTF16(sync_title);
  const BookmarkNode* child_node = BookmarkChangeProcessor::CreateBookmarkNode(
      bookmark_title, url, sync_child_node, parent_node, bookmark_model_,
      sync_client_, bookmark_index);
  if (!child_node) {
    *error = unrecoverable_error_handler_->CreateAndUploadError(
        FROM_HERE, "Failed to create bookmark node with title " + sync_title +
                       " and url " + url.possibly_invalid_spec(),
        model_type());
    return nullptr;
  }

  context->UpdateDuplicateCount(bookmark_title, url);
  return child_node;
}

struct FolderInfo {
  FolderInfo(const BookmarkNode* f, const BookmarkNode* p, int64_t id)
      : folder(f), parent(p), sync_id(id) {}
  const BookmarkNode* folder;
  const BookmarkNode* parent;
  int64_t sync_id;
};
using FolderInfoList = std::vector<FolderInfo>;

void BookmarkModelAssociator::ApplyDeletesFromSyncJournal(
    syncer::BaseTransaction* trans,
    Context* context) {
  syncer::BookmarkDeleteJournalList bk_delete_journals;
  syncer::DeleteJournal::GetBookmarkDeleteJournals(trans, &bk_delete_journals);
  if (bk_delete_journals.empty())
    return;

  size_t num_journals_unmatched = bk_delete_journals.size();

  // Make a set of all external IDs in the delete journal,
  // ignore entries with unset external IDs.
  std::set<int64_t> journaled_external_ids;
  for (size_t i = 0; i < num_journals_unmatched; i++) {
    if (bk_delete_journals[i].external_id != 0)
      journaled_external_ids.insert(bk_delete_journals[i].external_id);
  }

  // Check bookmark model from top to bottom.
  BookmarkStack dfs_stack;
  for (auto it = context->bookmark_roots().begin();
       it != context->bookmark_roots().end(); ++it) {
    dfs_stack.push(*it);
  }

  // Remember folders that match delete journals in first pass but don't delete
  // them in case there are bookmarks left under them. After non-folder
  // bookmarks are removed in first pass, recheck the folders in reverse order
  // to remove empty ones.
  FolderInfoList folders_matched;
  while (!dfs_stack.empty() && num_journals_unmatched > 0) {
    const BookmarkNode* parent = dfs_stack.top();
    dfs_stack.pop();
    DCHECK(parent->is_folder());

    // Enumerate folder children in reverse order to make it easier to remove
    // bookmarks matching entries in the delete journal.
    for (int child_index = parent->child_count() - 1;
         child_index >= 0 && num_journals_unmatched > 0; --child_index) {
      const BookmarkNode* child = parent->GetChild(child_index);
      if (child->is_folder())
        dfs_stack.push(child);

      if (journaled_external_ids.find(child->id()) ==
          journaled_external_ids.end()) {
        // Skip bookmark node which id is not in the set of external IDs.
        continue;
      }

      // Iterate through the journal entries from back to front. Remove matched
      // journal by moving an unmatched entry at the tail to the matched
      // position so that we can read unmatched entries off the head in next
      // loop.
      for (int journal_index = num_journals_unmatched - 1; journal_index >= 0;
           --journal_index) {
        const syncer::BookmarkDeleteJournal& delete_entry =
            bk_delete_journals[journal_index];
        if (child->id() == delete_entry.external_id &&
            BookmarkNodeFinder::NodeMatches(
                child, GURL(delete_entry.specifics.bookmark().url()),
                delete_entry.specifics.bookmark().title(),
                delete_entry.is_folder)) {
          if (child->is_folder()) {
            // Remember matched folder without removing and delete only empty
            // ones later.
            folders_matched.push_back(
                FolderInfo(child, parent, delete_entry.id));
          } else {
            bookmark_model_->Remove(child);
            context->IncrementLocalItemsDeleted();
          }
          // Move unmatched journal here and decrement counter.
          bk_delete_journals[journal_index] =
              bk_delete_journals[--num_journals_unmatched];
          break;
        }
      }
    }
  }

  // Ids of sync nodes not found in bookmark model, meaning the deletions are
  // persisted and correponding delete journals can be dropped.
  std::set<int64_t> journals_to_purge;

  // Remove empty folders from bottom to top.
  for (auto it = folders_matched.rbegin(); it != folders_matched.rend(); ++it) {
    if (it->folder->child_count() == 0) {
      bookmark_model_->Remove(it->folder);
      context->IncrementLocalItemsDeleted();
    } else {
      // Keep non-empty folder and remove its journal so that it won't match
      // again in the future.
      journals_to_purge.insert(it->sync_id);
    }
  }

  // Purge unmatched journals.
  for (size_t i = 0; i < num_journals_unmatched; ++i)
    journals_to_purge.insert(bk_delete_journals[i].id);
  syncer::DeleteJournal::PurgeDeleteJournals(trans, journals_to_purge);
}

void BookmarkModelAssociator::PostPersistAssociationsTask() {
  // No need to post a task if a task is already pending.
  if (weak_factory_.HasWeakPtrs())
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BookmarkModelAssociator::PersistAssociations,
                                weak_factory_.GetWeakPtr()));
}

void BookmarkModelAssociator::PersistAssociations() {
  // If there are no dirty associations we have nothing to do. We handle this
  // explicity instead of letting the for loop do it to avoid creating a write
  // transaction in this case.
  if (dirty_associations_sync_ids_.empty()) {
    DCHECK(id_map_.empty());
    DCHECK(id_map_inverse_.empty());
    return;
  }

  int64_t new_version = syncer::syncable::kInvalidTransactionVersion;
  std::vector<const BookmarkNode*> bnodes;
  {
    syncer::WriteTransaction trans(FROM_HERE, user_share_, &new_version);
    DirtyAssociationsSyncIds::iterator iter;
    for (iter = dirty_associations_sync_ids_.begin();
         iter != dirty_associations_sync_ids_.end();
         ++iter) {
      int64_t sync_id = *iter;
      syncer::WriteNode sync_node(&trans);
      if (sync_node.InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK) {
        syncer::SyncError error(
            FROM_HERE,
            syncer::SyncError::DATATYPE_ERROR,
            "Could not lookup bookmark node for ID persistence.",
            syncer::BOOKMARKS);
        unrecoverable_error_handler_->OnUnrecoverableError(error);
        return;
      }
      const BookmarkNode* node = GetChromeNodeFromSyncId(sync_id);
      if (node && sync_node.GetExternalId() != node->id()) {
        sync_node.SetExternalId(node->id());
        bnodes.push_back(node);
      }
    }
    dirty_associations_sync_ids_.clear();
  }

  BookmarkChangeProcessor::UpdateTransactionVersion(new_version,
                                                    bookmark_model_,
                                                    bnodes);
}

bool BookmarkModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  const syncer::ModelTypeSet encrypted_types = trans.GetEncryptedTypes();
  return !encrypted_types.Has(syncer::BOOKMARKS) ||
      trans.GetCryptographer()->is_ready();
}

syncer::SyncError BookmarkModelAssociator::CheckModelSyncState(
    Context* context) const {
  DCHECK_EQ(context->native_model_sync_state(), UNSET);
  int64_t native_version =
      bookmark_model_->root_node()->sync_transaction_version();

  syncer::ReadTransaction trans(FROM_HERE, user_share_);
  int64_t sync_version = trans.GetModelVersion(syncer::BOOKMARKS);
  context->SetPreAssociationVersions(native_version, sync_version);

  if (native_version != syncer::syncable::kInvalidTransactionVersion) {
    if (native_version == sync_version) {
      context->set_native_model_sync_state(IN_SYNC);
    } else {
      // TODO(wychen): enum uma should be strongly typed. crbug.com/661401
      UMA_HISTOGRAM_ENUMERATION("Sync.LocalModelOutOfSync",
                                ModelTypeToHistogramInt(syncer::BOOKMARKS),
                                static_cast<int>(syncer::MODEL_TYPE_COUNT));

      // Clear version on bookmark model so that we only report error once.
      bookmark_model_->SetNodeSyncTransactionVersion(
          bookmark_model_->root_node(),
          syncer::syncable::kInvalidTransactionVersion);

      // If the native version is higher, there was a sync persistence failure,
      // and we need to delay association until after a GetUpdates.
      if (native_version > sync_version) {
        context->set_native_model_sync_state(AHEAD);
        std::string message = base::StringPrintf(
            "Native version (%" PRId64 ") does not match sync version (%"
                PRId64 ")",
            native_version,
            sync_version);
        return syncer::SyncError(FROM_HERE,
                                 syncer::SyncError::PERSISTENCE_ERROR,
                                 message,
                                 syncer::BOOKMARKS);
      } else {
        context->set_native_model_sync_state(BEHIND);
      }
    }
  }
  return syncer::SyncError();
}

}  // namespace sync_bookmarks
