// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/string_compare.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/typed_count_sorter.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/browser/url_index.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/favicon_base/favicon_types.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"

using base::Time;

namespace bookmarks {

namespace {

bool AreFoldersForAccountStorageAllowed() {
  return base::FeatureList::IsEnabled(
      syncer::kSyncEnableBookmarksInTransportMode);
}

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

// Traverses ancestors to find a permanent node or null in the rare case where
// the node has no ancestor permanent node. This can happen if `node` is the
// root node or because `node` is in the process of being deleted (i.e. removed
// from the indices), typically as a result of feature code reacting to
// BookmarkModelObserver::BookmarkNodeRemoved().
const BookmarkNode* GetSelfOrAncestorPermanentNode(const BookmarkNode* node) {
  CHECK(node);
  while (node && !node->is_permanent_node()) {
    node = node->parent();
  }
  return node;
}

// Comparator used when sorting permanent nodes. Nodes that are initially
// visible are sorted before nodes that are initially hidden.
class VisibilityComparator {
 public:
  explicit VisibilityComparator(BookmarkClient* client) : client_(client) {}

  // Returns true if `n1` precedes `n2`.
  bool operator()(const std::unique_ptr<BookmarkNode>& n1,
                  const std::unique_ptr<BookmarkNode>& n2) {
    DCHECK(n1->is_permanent_node());
    DCHECK(n2->is_permanent_node());
    bool n1_visible = BookmarkPermanentNode::IsTypeVisibleWhenEmpty(n1->type());
    bool n2_visible = BookmarkPermanentNode::IsTypeVisibleWhenEmpty(n2->type());
    return n1_visible != n2_visible && n1_visible;
  }

 private:
  raw_ptr<BookmarkClient> client_;
};

// Comparator used when sorting bookmarks. Folders are sorted first, then
// bookmarks.
class SortComparator {
 public:
  explicit SortComparator(icu::Collator* collator) : collator_(collator) {}

  // Returns true if `n1` precedes `n2`.
  bool operator()(const std::unique_ptr<BookmarkNode>& n1,
                  const std::unique_ptr<BookmarkNode>& n2) {
    if (n1->type() == n2->type()) {
      // Types are the same, compare the names.
      if (!collator_) {
        return n1->GetTitle() < n2->GetTitle();
      }
      return base::i18n::CompareString16WithCollator(
                 *collator_, n1->GetTitle(), n2->GetTitle()) == UCOL_LESS;
    }
    // Types differ, sort such that folders come first.
    return n1->is_folder();
  }

 private:
  raw_ptr<icu::Collator> collator_;
};

}  // namespace

// BookmarkModel --------------------------------------------------------------

BookmarkModel::BookmarkModel(std::unique_ptr<BookmarkClient> client)
    : owned_root_(std::make_unique<BookmarkNode>(
          /*id=*/0,
          base::Uuid::ParseLowercase(kRootNodeUuid),
          GURL())),
      root_(owned_root_.get()),
      observers_(base::ObserverListPolicy::EXISTING_ONLY),
      client_(std::move(client)) {
  DCHECK(client_);
  uuid_index_.emplace(NodeTypeForUuidLookup::kLocalOrSyncableNodes,
                      UuidIndex());
  uuid_index_.emplace(NodeTypeForUuidLookup::kAccountNodes, UuidIndex());
  client_->Init(this);
}

BookmarkModel::~BookmarkModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkModelBeingDeleted();
  }

  // The stores maintain a reference back to us. Destroy them early so that they
  // don't try and invoke a method back on `this` again.
  local_or_syncable_store_.reset();
  account_store_.reset();

  // `TitledUrlIndex` owns  a `TypedCountSorter` that keeps a raw_ptr to the
  // client. So titled_url_index_ must be reset first.
  titled_url_index_.reset();

  // ChromeBookmarkClient indirectly observes the model. The client should thus
  // be reset before the observer list.
  client_.reset();

  // Set raw_ptr values to null to avoid danling pointer detection when UrlIndex
  // is destroyed.
  account_bookmark_bar_node_ = nullptr;
  account_other_node_ = nullptr;
  account_mobile_node_ = nullptr;
}

void BookmarkModel::Load(const base::FilePath& profile_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the stores are non-null, it means Load was already invoked. Load should
  // only be invoked once.
  CHECK(!local_or_syncable_store_);
  CHECK(!account_store_);

  const base::FilePath local_or_syncable_file_path =
      profile_path.Append(kLocalOrSyncableBookmarksFileName);

  const base::FilePath account_file_path =
      AreFoldersForAccountStorageAllowed()
          ? profile_path.Append(kAccountBookmarksFileName)
          : base::FilePath();

  local_or_syncable_store_ = std::make_unique<BookmarkStorage>(
      this, BookmarkStorage::kSelectLocalOrSyncableNodes,
      local_or_syncable_file_path);

  if (!account_file_path.empty()) {
    account_store_ = std::make_unique<BookmarkStorage>(
        this, BookmarkStorage::kSelectAccountNodes, account_file_path);
  }

  // Creating ModelLoader schedules the load on a backend task runner.
  model_loader_ = ModelLoader::Create(
      local_or_syncable_file_path, account_file_path,
      client_->GetLoadManagedNodeCallback(),
      base::BindOnce(&BookmarkModel::DoneLoading, AsWeakPtr()));
}

scoped_refptr<ModelLoader> BookmarkModel::model_loader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_loader_;
}

const BookmarkNode* BookmarkModel::account_bookmark_bar_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must be null if the feature flag isn't enabled.
  CHECK(!account_bookmark_bar_node_ || AreFoldersForAccountStorageAllowed());
  return account_bookmark_bar_node_;
}

const BookmarkNode* BookmarkModel::account_other_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must be null if the feature flag isn't enabled.
  CHECK(!account_other_node_ || AreFoldersForAccountStorageAllowed());
  return account_other_node_;
}

const BookmarkNode* BookmarkModel::account_mobile_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must be null if the feature flag isn't enabled.
  CHECK(!account_mobile_node_ || AreFoldersForAccountStorageAllowed());
  return account_mobile_node_;
}

bool BookmarkModel::IsLocalOnlyNode(const BookmarkNode& node) const {
  if (is_root_node(&node)) {
    // The semantics aren't clear for the root, but returning true seems most
    // sensible as the root is a synthetic node that doesn't get uploaded to
    // servers.
    return true;
  }

  const BookmarkNode* ancestor_permanent_node =
      GetSelfOrAncestorPermanentNode(&node);
  if (!ancestor_permanent_node) {
    // In rare cases, `node` may already be 'dettached' from the bookmark tree.
    // This can happen for example if this function is exercised as a reaction
    // to BookmarkModelObserver::BookmarkNodeRemoved(). In this case, the
    // semantics of this function aren't clear, but following the same rationale
    // as for the root node, discussed above, returning true seems most
    // sensible.
    return true;
  }

  if (client_->IsNodeManaged(ancestor_permanent_node)) {
    // Managed nodes don't sync.
    return true;
  }

  if (client_->IsSyncFeatureEnabledIncludingBookmarks()) {
    // If sync-the-feature is on, including bookmarks, then there is no
    // separation between local and account bookmarks, and all bookmarks are
    // getting sync-ed to the server.
    return false;
  }

  // If sync is off, the only remaining possibility to return false is if `node`
  // is actually a descendant of an account permanent folder (if they exist).
  return ancestor_permanent_node != account_bookmark_bar_node_ &&
         ancestor_permanent_node != account_other_node_ &&
         ancestor_permanent_node != account_mobile_node_;
}

void BookmarkModel::AddObserver(BookmarkModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void BookmarkModel::RemoveObserver(BookmarkModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void BookmarkModel::BeginExtensiveChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (++extensive_changes_ == 1) {
    for (BookmarkModelObserver& observer : observers_) {
      observer.ExtensiveBookmarkChangesBeginning();
    }
  }
}

void BookmarkModel::EndExtensiveChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    for (BookmarkModelObserver& observer : observers_) {
      observer.ExtensiveBookmarkChangesEnded();
    }
  }
}

void BookmarkModel::BeginGroupedChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_) {
    observer.GroupedBookmarkChangesBeginning();
  }
}

void BookmarkModel::EndGroupedChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_) {
    observer.GroupedBookmarkChangesEnded();
  }
}

void BookmarkModel::Remove(const BookmarkNode* node,
                           metrics::BookmarkEditSource source,
                           const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_root_node(node));
  const BookmarkNode* parent = node->parent();
  DCHECK(parent);
  std::optional<size_t> index = parent->GetIndexOf(node);
  DCHECK(index.has_value());

  // Removing a permanent node is problematic and can cause crashes elsewhere
  // that are difficult to trace back.
  CHECK(!is_permanent_node(node)) << "for type " << node->type();

  std::unique_ptr<BookmarkNode> owned_node = RemoveNode(node, location);

  client_->OnBookmarkNodeRemovedUndoable(parent, index.value(),
                                         std::move(owned_node));

  metrics::RecordBookmarkRemoved(source);
}

void BookmarkModel::RemoveAllUserBookmarks(const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  std::set<GURL> removed_urls;
  struct RemoveNodeData {
    raw_ptr<const BookmarkNode> parent;
    int index;
    std::unique_ptr<BookmarkNode> node;
  };
  std::vector<RemoveNodeData> removed_node_data_list;

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillRemoveAllUserBookmarks(location);
  }

  BeginExtensiveChanges();
  // Skip deleting permanent nodes. Permanent bookmark nodes are the root and
  // its immediate children. For removing all non permanent nodes just remove
  // all children of non-root permanent nodes.
  {
    for (const auto& permanent_node : root_->children()) {
      if (client_->IsNodeManaged(permanent_node.get())) {
        continue;
      }

      const NodeTypeForUuidLookup type_for_uuid_lookup =
          DetermineTypeForUuidLookupForExistingNode(permanent_node.get());

      for (int j = static_cast<int>(permanent_node->children().size() - 1);
           j >= 0; --j) {
        std::unique_ptr<BookmarkNode> node = url_index_->Remove(
            permanent_node->children()[j].get(), &removed_urls);
        RemoveNodeFromIndicesRecursive(node.get(), type_for_uuid_lookup);
        removed_node_data_list.push_back(
            {permanent_node.get(), j, std::move(node)});
      }

      // Note that scheduling redundant saves is a no-op so it's done here
      // inside the loop for simplicity.
      ScheduleSaveForNode(permanent_node.get());
    }
  }

  EndExtensiveChanges();

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkAllUserNodesRemoved(removed_urls, location);
  }

  BeginGroupedChanges();
  for (auto& removed_node_data : removed_node_data_list) {
    client_->OnBookmarkNodeRemovedUndoable(removed_node_data.parent,
                                           removed_node_data.index,
                                           std::move(removed_node_data.node));
  }
  EndGroupedChanges();
}

void BookmarkModel::Move(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(node->HasAncestor(root_node()));
  CHECK(new_parent->HasAncestor(root_node()));
  DCHECK(IsValidIndex(new_parent, index, true));
  DCHECK(!is_root_node(new_parent));
  DCHECK(!is_permanent_node(node));
  DCHECK(!new_parent->HasAncestor(node));

  SCOPED_CRASH_KEY_NUMBER("BookmarkModelMove", "newParentType",
                          new_parent->type());
  DUMP_WILL_BE_CHECK(new_parent->is_folder());

  const BookmarkNode* old_parent = node->parent();
  size_t old_index = old_parent->GetIndexOf(node).value();

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return;
  }

  SetDateFolderModified(new_parent, Time::Now());

  if (old_parent == new_parent && index > old_index) {
    index--;
  }

  const NodeTypeForUuidLookup old_type_for_uuid_lookup =
      DetermineTypeForUuidLookupForExistingNode(old_parent);
  const NodeTypeForUuidLookup new_type_for_uuid_lookup =
      DetermineTypeForUuidLookupForExistingNode(new_parent);

  if (old_type_for_uuid_lookup != new_type_for_uuid_lookup) {
    uuid_index_[old_type_for_uuid_lookup].erase(node);

    bool success = uuid_index_[new_type_for_uuid_lookup].insert(node).second;

    if (!success) {
      // It is possible that the UUID exists in the new index. In this case, to
      // avoid the collision, it is necessary to assign a new UUID.
      AsMutable(node)->SetNewRandomUuid();
      CHECK(uuid_index_[new_type_for_uuid_lookup].insert(node).second);
    }
  }

  BookmarkNode* mutable_old_parent = AsMutable(old_parent);
  std::unique_ptr<BookmarkNode> owned_node =
      mutable_old_parent->Remove(old_index);
  BookmarkNode* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  // These two calls don't guarantee that they get scheduled at the same time,
  // which increases the risk that, if two JSON files are involved in this move,
  // only one of them may succeed to write to disk (leading to data loss or data
  // duplication). These scenarios should be very rare and the scheduling aspect
  // of it is only a smart part of it, so we don't bother being too smart about
  // it. Other risks are inherent to the use of two files.
  ScheduleSaveForNode(old_parent);
  ScheduleSaveForNode(new_parent);

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeMoved(old_parent, old_index, new_parent, index);
  }

  if (old_parent != new_parent) {
    // TODO(crbug.com/40074470): Remove if check once the root cause of this
    // crash is identified and addressed, and new_parent->is_folder() is
    // checked at the top of this method.
    if (new_parent->is_folder()) {
      metrics::RecordBookmarkMovedTo(GetFolderType(new_parent));
    }
  }
}

void BookmarkModel::UpdateLastUsedTime(const BookmarkNode* node,
                                       const base::Time time,
                                       bool just_opened) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);

  base::Time last_used_time = node->date_last_used();
  UpdateLastUsedTimeImpl(node, time);
  if (just_opened) {
    metrics::RecordBookmarkOpened(time, last_used_time, node->date_added(),
                                  GetStorageStateForUma(node));
  }
}

void BookmarkModel::UpdateLastUsedTimeImpl(const BookmarkNode* node,
                                           const base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);

  BookmarkNode* mutable_node = AsMutable(node);
  mutable_node->set_date_last_used(time);

  ScheduleSaveForNode(node);
}

void BookmarkModel::ClearLastUsedTimeInRange(const base::Time delete_begin,
                                             const base::Time delete_end) {
  ClearLastUsedTimeInRangeRecursive(root_, delete_begin, delete_end);
}

void BookmarkModel::ClearLastUsedTimeInRangeRecursive(
    BookmarkNode* node,
    const base::Time delete_begin,
    const base::Time delete_end) {
  bool within_range = node->date_last_used() >= delete_begin &&
                      node->date_last_used() < delete_end;
  bool for_all_time =
      delete_begin.is_null() && (delete_end.is_null() || delete_end.is_max());
  if (node->is_url() && (within_range || for_all_time)) {
    UpdateLastUsedTimeImpl(node, Time());
  }

  for (size_t i = 0; i < node->children().size(); ++i) {
    ClearLastUsedTimeInRangeRecursive(node->children()[i].get(), delete_begin,
                                      delete_end);
  }
}

void BookmarkModel::Copy(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(IsValidIndex(new_parent, index, true));
  DCHECK(!is_root_node(new_parent));
  DCHECK(!is_permanent_node(node));
  DCHECK(!new_parent->HasAncestor(node));
  DCHECK(node->HasAncestor(root_node()));
  DCHECK(new_parent->HasAncestor(root_node()));

  SetDateFolderModified(new_parent, Time::Now());
  BookmarkNodeData drag_data(node);
  // CloneBookmarkNode will use BookmarkModel methods to do the job, so we
  // don't need to send notifications here or schedule a save.
  CloneBookmarkNode(this, drag_data.elements, new_parent, index, true);
}

const gfx::Image& BookmarkModel::GetFavicon(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  if (node->favicon_state() == BookmarkNode::INVALID_FAVICON) {
    BookmarkNode* mutable_node = AsMutable(node);
    LoadFavicon(mutable_node);
  }
  return node->favicon();
}

void BookmarkModel::SetTitle(const BookmarkNode* node,
                             const std::u16string& title,
                             metrics::BookmarkEditSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);

  if (node->GetTitle() == title) {
    return;
  }

  if (is_permanent_node(node) && !client_->CanSetPermanentNodeTitle(node)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkNode(node);
  }

  // The title index doesn't support changing the title, instead we remove then
  // add it back. Only do this for URL nodes. A directory node can have its
  // title changed but should be excluded from the index.
  if (node->is_url()) {
    titled_url_index_->Remove(node);
  } else {
    titled_url_index_->RemovePath(node);
  }
  url_index_->SetTitle(AsMutable(node), title);
  if (node->is_url()) {
    titled_url_index_->Add(node);
  } else {
    titled_url_index_->AddPath(node);
  }

  ScheduleSaveForNode(node);

  metrics::RecordTitleEdit(source);
  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChanged(node);
  }
}

void BookmarkModel::SetURL(const BookmarkNode* node,
                           const GURL& url,
                           metrics::BookmarkEditSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  DCHECK(!node->is_folder());

  if (node->url() == url) {
    return;
  }

  BookmarkNode* mutable_node = AsMutable(node);
  mutable_node->InvalidateFavicon();
  CancelPendingFaviconLoadRequests(mutable_node);

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkNode(node);
  }

  // The title index doesn't support changing the URL, instead we remove then
  // add it back.
  titled_url_index_->Remove(mutable_node);
  url_index_->SetUrl(mutable_node, url);
  titled_url_index_->Add(mutable_node);

  ScheduleSaveForNode(node);

  metrics::RecordURLEdit(source);
  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChanged(node);
  }
}

void BookmarkModel::SetNodeMetaInfo(const BookmarkNode* node,
                                    const std::string& key,
                                    const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(node);
  CHECK(!is_root_node(node));

  std::string old_value;
  if (node->GetMetaInfo(key, &old_value) && old_value == value) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkMetaInfo(node);
  }

  if (AsMutable(node)->SetMetaInfo(key, value)) {
    ScheduleSaveForNode(node);
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkMetaInfoChanged(node);
  }
}

void BookmarkModel::SetNodeMetaInfoMap(
    const BookmarkNode* node,
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(node);
  CHECK(!is_root_node(node));

  const BookmarkNode::MetaInfoMap* old_meta_info_map = node->GetMetaInfoMap();
  if ((!old_meta_info_map && meta_info_map.empty()) ||
      (old_meta_info_map && meta_info_map == *old_meta_info_map)) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkMetaInfo(node);
  }

  AsMutable(node)->SetMetaInfoMap(meta_info_map);
  ScheduleSaveForNode(node);

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkMetaInfoChanged(node);
  }
}

void BookmarkModel::DeleteNodeMetaInfo(const BookmarkNode* node,
                                       const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (!meta_info_map || meta_info_map->find(key) == meta_info_map->end()) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkMetaInfo(node);
  }

  if (AsMutable(node)->DeleteMetaInfo(key)) {
    ScheduleSaveForNode(node);
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkMetaInfoChanged(node);
  }
}

void BookmarkModel::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                      const GURL& icon_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_) {
    return;
  }

  std::set<const BookmarkNode*> to_update;
  for (const GURL& page_url : page_urls) {
    std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
        GetNodesByURL(page_url);
    to_update.insert(nodes.begin(), nodes.end());
  }

  if (!icon_url.is_empty()) {
    // TODO(pkotwicz): Do something more efficient if `icon_url` is non-empty
    // many times a day for each user.
    url_index_->GetNodesWithIconUrl(icon_url, &to_update);
  }

  for (const BookmarkNode* node : to_update) {
    // Rerequest the favicon.
    BookmarkNode* mutable_node = AsMutable(node);
    mutable_node->InvalidateFavicon();
    CancelPendingFaviconLoadRequests(mutable_node);
    for (BookmarkModelObserver& observer : observers_) {
      observer.BookmarkNodeFaviconChanged(node);
    }
  }
}

void BookmarkModel::SetDateAdded(const BookmarkNode* node, Time date_added) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  DCHECK(!is_permanent_node(node));

  if (node->date_added() == date_added) {
    return;
  }

  AsMutable(node)->set_date_added(date_added);

  // Syncing might result in dates newer than the folder's last modified date.
  if (date_added > node->parent()->date_folder_modified()) {
    // Will trigger BookmarkStorage::ScheduleSaveForNode().
    SetDateFolderModified(node->parent(), date_added);
  } else {
    ScheduleSaveForNode(node);
  }
}

std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
BookmarkModel::GetNodesByURL(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;

  if (url_index_) {
    url_index_->GetNodesByUrl(url, &nodes);
  }

  return nodes;
}

const BookmarkNode* BookmarkModel::GetNodeByUuid(
    const base::Uuid& uuid,
    NodeTypeForUuidLookup type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Because of having to create a dummy node, the invalid-UUID case needs
  // special handling.
  if (!uuid.is_valid()) {
    return nullptr;
  }

  const UuidIndex& uuid_index = uuid_index_.at(type);
  auto it = uuid_index.find(uuid);
  return it == uuid_index.end() ? nullptr : *it;
}

const BookmarkNode* BookmarkModel::GetMostRecentlyAddedUserNodeForURL(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
      GetNodesByURL(url);
  std::sort(nodes.begin(), nodes.end(), &MoreRecentlyAdded);

  // Look for the first node that the user can edit.
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (!client_->IsNodeManaged(nodes[i])) {
      return nodes[i];
    }
  }

  return nullptr;
}

bool BookmarkModel::HasBookmarks() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_index_ && url_index_->HasBookmarks();
}

bool BookmarkModel::HasNoUserCreatedBookmarksOrFolders() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return bookmark_bar_node_->children().empty() &&
         other_node_->children().empty() && mobile_node_->children().empty();
}

bool BookmarkModel::IsBookmarked(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_index_ && url_index_->IsBookmarked(url);
}

std::vector<UrlAndTitle> BookmarkModel::GetUniqueUrls() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url_index_) {
    return {};
  }
  return url_index_->GetUniqueUrls();
}

metrics::BookmarkFolderTypeForUMA BookmarkModel::GetFolderType(
    const BookmarkNode* folder) const {
  CHECK(folder->is_folder());
  if (folder == bookmark_bar_node()) {
    return metrics::BookmarkFolderTypeForUMA::kBookmarksBar;
  } else if (folder == other_node()) {
    return metrics::BookmarkFolderTypeForUMA::kOtherBookmarks;
  } else if (folder == mobile_node()) {
    return metrics::BookmarkFolderTypeForUMA::kMobileBookmarks;
  }
  return metrics::BookmarkFolderTypeForUMA::kUserGeneratedFolder;
}

const BookmarkNode* BookmarkModel::AddFolder(
    const BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const BookmarkNode::MetaInfoMap* meta_info,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(parent);
  DCHECK(parent->is_folder());
  DCHECK(!is_root_node(parent));
  DCHECK(parent->HasAncestor(root_node()));
  DCHECK(IsValidIndex(parent, index, true));
  DCHECK(!uuid || uuid->is_valid());

  const base::Time provided_creation_time_or_now =
      creation_time.value_or(Time::Now());

  auto new_node = std::make_unique<BookmarkNode>(
      generate_next_node_id(), uuid.value_or(base::Uuid::GenerateRandomV4()),
      GURL());
  new_node->set_date_added(provided_creation_time_or_now);
  new_node->set_date_folder_modified(provided_creation_time_or_now);
  // Folders shouldn't have line breaks in their titles.
  new_node->SetTitle(title);
  if (meta_info) {
    new_node->SetMetaInfoMap(*meta_info);
  }
  metrics::RecordBookmarkFolderAdded(GetFolderType(parent),
                                     GetStorageStateForUma(parent));
  // TODO(mastiz): `added_by_user` should be true below or the parameter
  // renamed.
  return AddNode(AsMutable(parent), index, std::move(new_node),
                 /*added_by_user=*/false,
                 DetermineTypeForUuidLookupForExistingNode(parent));
}

const BookmarkNode* BookmarkModel::AddNewURL(
    const BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const BookmarkNode::MetaInfoMap* meta_info) {
  metrics::RecordUrlBookmarkAdded(GetFolderType(parent),
                                  GetStorageStateForUma(parent));
  return AddURL(parent, index, title, url, meta_info, std::nullopt,
                std::nullopt, true);
}

const BookmarkNode* BookmarkModel::AddURL(
    const BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const BookmarkNode::MetaInfoMap* meta_info,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid,
    bool added_by_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(url.is_valid());
  DCHECK(parent);
  DCHECK(parent->is_folder());
  DCHECK(!is_root_node(parent));
  DCHECK(parent->HasAncestor(root_node()));
  DCHECK(IsValidIndex(parent, index, true));
  DCHECK(!uuid || uuid->is_valid());

  const base::Time provided_creation_time_or_now =
      creation_time.value_or(Time::Now());

  // Syncing may result in dates newer than the last modified date.
  if (provided_creation_time_or_now > parent->date_folder_modified()) {
    SetDateFolderModified(parent, provided_creation_time_or_now);
  }

  auto new_node = std::make_unique<BookmarkNode>(
      generate_next_node_id(), uuid.value_or(base::Uuid::GenerateRandomV4()),
      url);
  new_node->SetTitle(title);
  new_node->set_date_added(provided_creation_time_or_now);
  if (meta_info) {
    new_node->SetMetaInfoMap(*meta_info);
  }

  return AddNode(AsMutable(parent), index, std::move(new_node), added_by_user,
                 DetermineTypeForUuidLookupForExistingNode(parent));
}

void BookmarkModel::SortChildren(const BookmarkNode* parent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_->IsNodeManaged(parent));

  if (!parent || !parent->is_folder() || is_root_node(parent) ||
      parent->children().size() <= 1) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillReorderBookmarkNode(parent);
  }

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error)) {
    collator.reset(nullptr);
  }

  AsMutable(parent)->SortChildren(SortComparator(collator.get()));

  ScheduleSaveForNode(parent);

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChildrenReordered(parent);
  }
}

void BookmarkModel::ReorderChildren(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& ordered_nodes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_->IsNodeManaged(parent));

  // Ensure that all children in `parent` are in `ordered_nodes`.
  DCHECK_EQ(parent->children().size(), ordered_nodes.size());
  for (const BookmarkNode* node : ordered_nodes) {
    DCHECK_EQ(parent, node->parent());
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillReorderBookmarkNode(parent);
  }

  if (ordered_nodes.size() > 1) {
    std::map<const BookmarkNode*, int> order;
    for (size_t i = 0; i < ordered_nodes.size(); ++i) {
      order[ordered_nodes[i]] = i;
    }

    std::vector<size_t> new_order(ordered_nodes.size());
    for (size_t old_index = 0; old_index < parent->children().size();
         ++old_index) {
      const BookmarkNode* node = parent->children()[old_index].get();
      size_t new_index = order[node];
      new_order[old_index] = new_index;
    }

    AsMutable(parent)->ReorderChildren(new_order);

    ScheduleSaveForNode(parent);
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChildrenReordered(parent);
  }
}

void BookmarkModel::SetDateFolderModified(const BookmarkNode* parent,
                                          const Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(parent);
  AsMutable(parent)->set_date_folder_modified(time);

  ScheduleSaveForNode(parent);
}

void BookmarkModel::ResetDateFolderModified(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDateFolderModified(node, Time());
}

std::vector<TitledUrlMatch> BookmarkModel::GetBookmarksMatching(
    const std::u16string& query,
    size_t max_count_hint,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_) {
    return {};
  }

  return titled_url_index_->GetResultsMatching(query, max_count_hint,
                                               matching_algorithm);
}

void BookmarkModel::DisableWritesToDiskForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_or_syncable_store_.reset();
  account_store_.reset();
}

void BookmarkModel::LoadEmptyForTest() {
  auto details = std::make_unique<BookmarkLoadDetails>();
  model_loader_ = ModelLoader::CreateForTest(
      client_->GetLoadManagedNodeCallback(), details.get());
  DoneLoading(std::move(details));
  CHECK(loaded_);
}

void BookmarkModel::CommitPendingWriteForTest() {
  if (local_or_syncable_store_) {
    local_or_syncable_store_->SaveNowIfScheduledForTesting();  // IN-TEST
  }
  if (account_store_) {
    account_store_->SaveNowIfScheduledForTesting();  // IN-TEST
  }
}

bool BookmarkModel::LocalOrSyncableStorageHasPendingWriteForTest() const {
  return local_or_syncable_store_->HasScheduledSaveForTesting();  // IN-TEST
}

bool BookmarkModel::AccountStorageHasPendingWriteForTest() const {
  CHECK(account_store_);
  return account_store_->HasScheduledSaveForTesting();  // IN-TEST
}

void BookmarkModel::RestoreRemovedNode(const BookmarkNode* parent,
                                       size_t index,
                                       std::unique_ptr<BookmarkNode> node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BookmarkNode* node_ptr = node.get();
  AddNode(AsMutable(parent), index, std::move(node), /*added_by_user=*/false,
          DetermineTypeForUuidLookupForExistingNode(parent));

  // We might be restoring a folder node that have already contained a set of
  // child nodes. We need to notify all of them.
  NotifyNodeAddedForAllDescendants(node_ptr, /*added_by_user=*/false);
}

BookmarkModel::NodeTypeForUuidLookup
BookmarkModel::DetermineTypeForUuidLookupForExistingNode(
    const BookmarkNode* node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!is_root_node(node));

  for (const auto& type_and_uuid_index : uuid_index_) {
    if (GetNodeByUuid(node->uuid(), type_and_uuid_index.first) == node) {
      return type_and_uuid_index.first;
    }
  }

  NOTREACHED();
}

void BookmarkModel::NotifyNodeAddedForAllDescendants(const BookmarkNode* node,
                                                     bool added_by_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (size_t i = 0; i < node->children().size(); ++i) {
    for (BookmarkModelObserver& observer : observers_) {
      observer.BookmarkNodeAdded(node, i, added_by_user);
    }
    NotifyNodeAddedForAllDescendants(node->children()[i].get(), added_by_user);
  }
}

void BookmarkModel::DoneLoading(std::unique_ptr<BookmarkLoadDetails> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(details);
  DCHECK(!loaded_);
  DCHECK(details->required_recovery() || !details->ids_reassigned());

  next_node_id_ = details->max_id();
  titled_url_index_ = details->owned_titled_url_index();
  uuid_index_[NodeTypeForUuidLookup::kLocalOrSyncableNodes] =
      details->owned_local_or_syncable_uuid_index();
  uuid_index_[NodeTypeForUuidLookup::kAccountNodes] =
      details->owned_account_uuid_index();
  url_index_ = details->url_index();
  root_ = url_index_->root();
  // See declaration for details on why `owned_root_` is reset.
  owned_root_.reset();
  bookmark_bar_node_ = details->bb_node();
  other_node_ = details->other_folder_node();
  mobile_node_ = details->mobile_folder_node();

  account_bookmark_bar_node_ = details->account_bb_node();
  account_other_node_ = details->account_other_folder_node();
  account_mobile_node_ = details->account_mobile_folder_node();

  if (!AreFoldersForAccountStorageAllowed()) {
    CHECK(!account_bookmark_bar_node_);
    CHECK(!account_other_node_);
    CHECK(!account_mobile_node_);
    CHECK(uuid_index_[NodeTypeForUuidLookup::kAccountNodes].empty());
  }

  titled_url_index_->SetNodeSorter(
      std::make_unique<TypedCountSorter>(client_.get()));
  // Sorting the permanent nodes has to happen on the main thread, so we do it
  // here, after loading completes.
  root_->SortChildren(VisibilityComparator(client_.get()));

  // Decoding of sync metadata may invoke `RemoveAccountPermanentFolders()`,
  // which can lead to dangling raw_ptr members.
  details->ResetPermanentNodePointers();

  loaded_ = true;

  if (details->required_recovery()) {
    // If the from-disk loading went through a recovery (e.g. IDs were
    // reassigned due to collisions), it is best to save the result back to
    // disk so it won't keep happening upon every restart.
    if (local_or_syncable_store_) {
      local_or_syncable_store_->ScheduleSave();
    }

    if (account_store_) {
      CHECK(AreFoldersForAccountStorageAllowed());
      account_store_->ScheduleSave();
    }

    client_->RequiredRecoveryToLoad(
        details->local_or_syncable_reassigned_ids_per_old_id());
  }

  client_->DecodeLocalOrSyncableBookmarkSyncMetadata(
      details->local_or_syncable_sync_metadata_str(),
      local_or_syncable_store_
          ? base::BindRepeating(
                &BookmarkStorage::ScheduleSave,
                base::Unretained(local_or_syncable_store_.get()))
          : base::DoNothing());

  if (AreFoldersForAccountStorageAllowed()) {
    client_->DecodeAccountBookmarkSyncMetadata(
        details->account_sync_metadata_str(),
        account_store_
            ? base::BindRepeating(&BookmarkStorage::ScheduleSave,
                                  base::Unretained(account_store_.get()))
            : base::DoNothing());
  }

  const base::TimeDelta load_duration =
      base::TimeTicks::Now() - details->load_start();
  metrics::RecordTimeToLoadAtStartup(load_duration);

  // Notify our direct observers.
  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkModelLoaded(details->ids_reassigned());
  }
}

BookmarkNode* BookmarkModel::AddNode(
    BookmarkNode* parent,
    size_t index,
    std::unique_ptr<BookmarkNode> node,
    bool added_by_user,
    NodeTypeForUuidLookup type_for_uuid_lookup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BookmarkNode* node_ptr = node.get();
  url_index_->Add(parent, index, std::move(node));

  ScheduleSaveForNode(node_ptr);

  AddNodeToIndicesRecursive(node_ptr, type_for_uuid_lookup);

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeAdded(parent, index, added_by_user);
  }

  return node_ptr;
}

void BookmarkModel::AddNodeToIndicesRecursive(
    const BookmarkNode* node,
    NodeTypeForUuidLookup type_for_uuid_lookup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool uuid_is_unique = uuid_index_[type_for_uuid_lookup].insert(node).second;
  DUMP_WILL_BE_CHECK(uuid_is_unique);

  if (node->is_url()) {
    titled_url_index_->Add(node);
  } else {
    titled_url_index_->AddPath(node);
  }

  for (const auto& child : node->children()) {
    AddNodeToIndicesRecursive(child.get(), type_for_uuid_lookup);
  }
}

std::unique_ptr<BookmarkNode> BookmarkModel::RemoveNode(
    const BookmarkNode* node,
    const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_root_node(node));
  const BookmarkNode* parent = node->parent();
  DCHECK(parent);
  std::optional<size_t> index = parent->GetIndexOf(node);
  DCHECK(index.has_value());

  const NodeTypeForUuidLookup type_for_uuid_lookup =
      DetermineTypeForUuidLookupForExistingNode(node);

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillRemoveBookmarks(parent, index.value(), node, location);
  }

  // Schedule the save before actually removing the node for
  // `ScheduleSaveNode()` to determine which underlying storage is relevant.
  // `parent` could be used instead except for the case where permanent account
  // folders are removed, which also exercises this codepath.
  ScheduleSaveForNode(node);

  std::set<GURL> removed_urls;
  std::unique_ptr<BookmarkNode> owned_node =
      url_index_->Remove(AsMutable(node), &removed_urls);
  RemoveNodeFromIndicesRecursive(owned_node.get(), type_for_uuid_lookup);

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeRemoved(parent, index.value(), node, removed_urls,
                                 location);
  }

  return owned_node;
}

void BookmarkModel::RemoveNodeFromIndicesRecursive(
    BookmarkNode* node,
    NodeTypeForUuidLookup type_for_uuid_lookup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(!is_permanent_node(node));

  if (node->is_url()) {
    titled_url_index_->Remove(node);
  } else {
    titled_url_index_->RemovePath(node);
  }

  uuid_index_[type_for_uuid_lookup].erase(node);

  // Reset favicon state for the case when the `node` is restored.
  CancelPendingFaviconLoadRequests(node);
  node->InvalidateFavicon();

  // Recurse through children.
  for (size_t i = node->children().size(); i > 0; --i) {
    RemoveNodeFromIndicesRecursive(node->children()[i - 1].get(),
                                   type_for_uuid_lookup);
  }
}

bool BookmarkModel::IsValidIndex(const BookmarkNode* parent,
                                 size_t index,
                                 bool allow_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parent && parent->is_folder() &&
         (index < parent->children().size() ||
          (allow_end && index == parent->children().size()));
}

void BookmarkModel::OnFaviconDataAvailable(
    BookmarkNode* node,
    const favicon_base::FaviconImageResult& image_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);

  node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  node->set_favicon_state(BookmarkNode::LOADED_FAVICON);
  if (!image_result.image.IsEmpty()) {
    node->set_favicon(image_result.image);
    node->set_icon_url(image_result.icon_url);
    FaviconLoaded(node);
  } else {
    // No favicon available, but we still notify observers.
    FaviconLoaded(node);
  }
}

void BookmarkModel::LoadFavicon(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (node->is_folder()) {
    return;
  }

  DCHECK(node->url().is_valid());
  node->set_favicon_state(BookmarkNode::LOADING_FAVICON);
  base::CancelableTaskTracker::TaskId taskId =
      client_->GetFaviconImageForPageURL(
          node->url(),
          base::BindOnce(&BookmarkModel::OnFaviconDataAvailable,
                         base::Unretained(this), node),
          &cancelable_task_tracker_);
  if (taskId != base::CancelableTaskTracker::kBadTaskId) {
    node->set_favicon_load_task_id(taskId);
  }
}

void BookmarkModel::FaviconLoaded(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeFaviconChanged(node);
  }
}

void BookmarkModel::CancelPendingFaviconLoadRequests(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (node->favicon_load_task_id() != base::CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(node->favicon_load_task_id());
    node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  }
}

int64_t BookmarkModel::generate_next_node_id() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  return next_node_id_++;
}

void BookmarkModel::CreateAccountPermanentFolders() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(AreFoldersForAccountStorageAllowed());
  CHECK(loaded_);

  {
    std::unique_ptr<BookmarkPermanentNode> account_bookmark_bar_node =
        BookmarkPermanentNode::CreateBookmarkBar(next_node_id_++);
    account_bookmark_bar_node_ = account_bookmark_bar_node.get();
    AddNode(root_, root_->children().size(),
            std::move(account_bookmark_bar_node),
            /*added_by_user=*/false, NodeTypeForUuidLookup::kAccountNodes);
  }
  {
    std::unique_ptr<BookmarkPermanentNode> account_other_node =
        BookmarkPermanentNode::CreateOtherBookmarks(next_node_id_++);
    account_other_node_ = account_other_node.get();
    AddNode(root_, root_->children().size(), std::move(account_other_node),
            /*added_by_user=*/false, NodeTypeForUuidLookup::kAccountNodes);
  }
  {
    std::unique_ptr<BookmarkPermanentNode> account_mobile_node =
        BookmarkPermanentNode::CreateMobileBookmarks(next_node_id_++);
    account_mobile_node_ = account_mobile_node.get();
    AddNode(root_, root_->children().size(), std::move(account_mobile_node),
            /*added_by_user=*/false, NodeTypeForUuidLookup::kAccountNodes);
  }
}

void BookmarkModel::RemoveAccountPermanentFolders() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(AreFoldersForAccountStorageAllowed());
  CHECK(loaded_);

  // No-op if account permanent folders don't exist.
  if (!account_bookmark_bar_node_) {
    CHECK(!account_other_node_);
    CHECK(!account_mobile_node_);
    return;
  }

  CHECK(account_other_node_);
  CHECK(account_mobile_node_);

  // Make a copy of the pointers before deleting the nodes, to avoid raw_ptr
  // reporting dangling pointers.
  std::vector<BookmarkNode*> account_permanent_folders{
      account_mobile_node_, account_other_node_, account_bookmark_bar_node_};

  account_bookmark_bar_node_ = nullptr;
  account_other_node_ = nullptr;
  account_mobile_node_ = nullptr;

  for (const BookmarkNode* node : account_permanent_folders) {
    RemoveNode(node, FROM_HERE);
  }
}

void BookmarkModel::ScheduleSaveForNode(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BookmarkStorage* storage = GetStorageForNode(node);
  if (storage) {
    storage->ScheduleSave();
  }
}

BookmarkStorage* BookmarkModel::GetStorageForNode(const BookmarkNode* node) {
  CHECK(node);
  CHECK(!is_root_node(node));

  const BookmarkNode* permanent_node = GetSelfOrAncestorPermanentNode(node);
  CHECK(permanent_node);

  if (permanent_node == account_bookmark_bar_node_ ||
      permanent_node == account_other_node_ ||
      permanent_node == account_mobile_node_) {
    CHECK(AreFoldersForAccountStorageAllowed());
    return account_store_.get();
  }

  return local_or_syncable_store_.get();
}

metrics::StorageStateForUma BookmarkModel::GetStorageStateForUma(
    const BookmarkNode* node) const {
  CHECK(node);
  CHECK(!is_root_node(node));

  const BookmarkNode* permanent_node = GetSelfOrAncestorPermanentNode(node);
  CHECK(permanent_node);

  if (permanent_node == account_bookmark_bar_node_ ||
      permanent_node == account_other_node_ ||
      permanent_node == account_mobile_node_) {
    CHECK(AreFoldersForAccountStorageAllowed());
    return metrics::StorageStateForUma::kAccount;
  }

  // The ancestor is a local-or-syncable permanent folder.
  return client_->IsSyncFeatureEnabledIncludingBookmarks()
             ? metrics::StorageStateForUma::kSyncEnabled
             : metrics::StorageStateForUma::kLocalOnly;
}

}  // namespace bookmarks
