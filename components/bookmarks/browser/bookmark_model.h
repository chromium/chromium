// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_undo_provider.h"
#include "components/bookmarks/browser/uuid_index.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/storage_type.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base

namespace favicon_base {
struct FaviconImageResult;
}

namespace query_parser {
enum class MatchingAlgorithm;
}

namespace bookmarks {

class BookmarkCodecTest;
class BookmarkLoadDetails;
class BookmarkModelObserver;
class BookmarkStorage;
class ModelLoader;
class ScopedGroupBookmarkActions;
class TitledUrlIndex;
class UrlIndex;

struct UrlAndTitle;
struct TitledUrlMatch;

// BookmarkModel --------------------------------------------------------------

// BookmarkModel provides a directed acyclic graph of URLs and folders.
// Three graphs are provided for the three entry points: those on the 'bookmarks
// bar', those in the 'other bookmarks' folder and those in the 'mobile' folder.
//
// An observer may be attached to observe relevant events.
//
// You should NOT directly create a BookmarkModel, instead go through the
// BookmarkModelFactory.
//
// Marked final to prevent unintended subclassing.
// `MoveToOtherModelWithNewNodeIdsAndUuids` affects two instances, and assumes
// that both instances are `BookmarkModel`, not some subclasses.
class BookmarkModel final : public BookmarkUndoProvider,
                            public KeyedService,
                            public base::SupportsUserData {
 public:
  // `client` must not be null.
  explicit BookmarkModel(std::unique_ptr<BookmarkClient> client);

  BookmarkModel(const BookmarkModel&) = delete;
  BookmarkModel& operator=(const BookmarkModel&) = delete;

  ~BookmarkModel() override;

  // Triggers the loading of bookmarks, which is an asynchronous operation with
  // most heavy-lifting taking place in a background sequence. Upon completion,
  // loaded() will return true and observers will be notified via
  // BookmarkModelLoaded(). Uses different files depending on
  // `storage_type` to support local and account storages.
  // Please note that for the time being the local storage is also used when
  // sync is on.
  // TODO(crbug.com/1422201): Update the note above when the local storage is
  //                          no longer used for sync.
  void Load(const base::FilePath& profile_path, StorageType storage_type);

  // Returns true if the model finished loading.
  bool loaded() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return loaded_;
  }

  // Returns the object responsible for tracking loading.
  scoped_refptr<ModelLoader> model_loader();

  // Returns the root node. The 'bookmark bar' node and 'other' node are
  // children of the root node.
  const BookmarkNode* root_node() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return root_;
  }

  // Returns the 'bookmark bar' node for the local-or-syncable storage.
  // Local-or-syncable storage is used for syncing bookmarks *only* if
  // Sync-the-feature is enabled. After Sync-to-Signin migration is finished -
  // local-or-syncable storage (and this folder) will become purely local.
  // This is null until loaded.
  const BookmarkNode* bookmark_bar_node() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return bookmark_bar_node_;
  }

  // Returns the 'other' node for the local-or-syncable storage.
  // Local-or-syncable storage is used for syncing bookmarks *only* if
  // Sync-the-feature is enabled. After Sync-to-Signin migration is finished -
  // local-or-syncable storage (and this folder) will become purely local.
  // This is null until loaded.
  const BookmarkNode* other_node() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return other_node_;
  }

  // Returns the 'mobile' node for the local-or-syncable storage.
  // Local-or-syncable storage is used for syncing bookmarks *only* if
  // Sync-the-feature is enabled. After Sync-to-Signin migration is finished -
  // local-or-syncable storage (and this folder) will become purely local.
  // This is null until loaded.
  const BookmarkNode* mobile_node() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mobile_node_;
  }

  // Returns the 'bookmark bar' node for the account storage. This is null until
  // loaded or if the user is not signed in (or isn't opted into syncing
  // bookmarks in the account storage).
  const BookmarkNode* account_bookmark_bar_node() const;

  // Returns the 'other' node for the account storage. This is null until loaded
  // or if the user is not signed in (or isn't opted into syncing bookmarks in
  // the account storage).
  const BookmarkNode* account_other_node() const;

  // Returns the 'mobile' node for the account storage. This is null until
  // loaded or if the user is not signed in (or isn't opted into syncing
  // bookmarks in the account storage).
  const BookmarkNode* account_mobile_node() const;

  bool is_root_node(const BookmarkNode* node) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node == root_;
  }

  // Returns whether the given `node` is one of the permanent nodes - root node,
  // 'bookmark bar' node, 'other' node or 'mobile' node, or one of the root
  // nodes supplied by the `client_`.
  bool is_permanent_node(const BookmarkNode* node) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return node && (node == root_ || node->parent() == root_);
  }

  void AddObserver(BookmarkModelObserver* observer);
  void RemoveObserver(BookmarkModelObserver* observer);

  // Notifies the observers that an extensive set of changes is about to happen,
  // such as during import or sync, so they can delay any expensive UI updates
  // until it's finished.
  void BeginExtensiveChanges();
  void EndExtensiveChanges();

  // Returns true if this bookmark model is currently in a mode where extensive
  // changes might happen, such as for import and sync. This is helpful for
  // observers that are created after the mode has started, and want to check
  // state during their own initializer, such as the NTP.
  bool IsDoingExtensiveChanges() const { return extensive_changes_ > 0; }

  // Removes `node` from the model and deletes it. Removing a folder node
  // recursively removes all nodes. Observers are notified immediately. `node`
  // must not be a permanent node. The source of the removal is passed through
  // `source`.
  void Remove(const BookmarkNode* node, metrics::BookmarkEditSource source);

  // Removes all the non-permanent bookmark nodes that are editable by the user.
  // Observers are only notified when all nodes have been removed. There is no
  // notification for individual node removals.
  void RemoveAllUserBookmarks();

  // Moves `node` to `new_parent` and inserts it at the given `index`.
  //
  // Note: this might cause UUIDs to get reassigned for `node` or its
  // descendants, when the node is moved between local and account storages.
  void Move(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            size_t index);

  // Inserts a copy of `node` into `new_parent` at `index`.
  void Copy(const BookmarkNode* node,
            const BookmarkNode* new_parent,
            size_t index);

  // TODO(crbug.com/1453250): Change this function to be invoked on the
  //                          destination model rather than on the source one.
  //
  // Moves `node` to another instance of `BookmarkModel` as determined by
  // `dest_model`, where it is inserted under `dest_parent` as a last child.
  // If `node` is a folder, all descendants (if any) are also moved, maintaining
  // the same hierarchy.
  // Please note that `BookmarkNode` objects representing `node` itself and its
  // descendants are not reused. Instead, the hierarchy is cloned (and new IDs
  // are generated) and this cloned hierarchy is added to `dest_model`.
  //
  // `node` must belong to this model, while `dest_parent` must belong to
  // `dest_model` (which must be different from `this`).
  //
  // Returns a pointer to the new node in the destination model.
  //
  // Calling this will send `OnWillRemoveBookmarks` and `BookmarkNodeRemoved`
  // for observers of this model and `BookmarkNodeAdded` for observers of
  // `dest_model`.
  const BookmarkNode* MoveToOtherModelWithNewNodeIdsAndUuids(
      const BookmarkNode* node,
      BookmarkModel* dest_model,
      const BookmarkNode* dest_parent);

  // Returns the favicon for `node`. If the favicon has not yet been loaded,
  // a load will be triggered and the observer of the model notified when done.
  // This also means that, on return, the node's state is guaranteed to be
  // either LOADED_FAVICON (if it was already loaded prior to the call) or
  // LOADING_FAVICON (with the exception of folders, where the call is a no-op).
  const gfx::Image& GetFavicon(const BookmarkNode* node);

  // Sets the title of `node`.
  void SetTitle(const BookmarkNode* node,
                const std::u16string& title,
                metrics::BookmarkEditSource source);

  // Sets the URL of `node`.
  void SetURL(const BookmarkNode* node,
              const GURL& url,
              metrics::BookmarkEditSource source);

  // Sets the date added time of `node`.
  void SetDateAdded(const BookmarkNode* node, base::Time date_added);

  // Returns the set of nodes with the `url`.
  [[nodiscard]] std::vector<const BookmarkNode*> GetNodesByURL(
      const GURL& url) const;

  // Returns the node with the given UUID or null if no node exists with this
  // UUID. Please note that this doesn't return account bookmarks.
  // TODO(crbug.com/1494120): Add support for account bookmarks.
  const BookmarkNode* GetNodeByUuid(const base::Uuid& uuid) const;

  // Returns the most recently added user node for the `url`; urls from any
  // nodes that are not editable by the user are never returned by this call.
  // Returns NULL if `url` is not bookmarked.
  const BookmarkNode* GetMostRecentlyAddedUserNodeForURL(const GURL& url) const;

  // Returns true if there are bookmarks, otherwise returns false.
  bool HasBookmarks() const;

  // Returns true is there is no user created bookmarks or folders.
  bool HasNoUserCreatedBookmarksOrFolders() const;

  // Returns true if the specified URL is bookmarked.
  bool IsBookmarked(const GURL& url) const;

  // Return the set of bookmarked urls and their titles. This returns the unique
  // set of URLs. For example, if two bookmarks reference the same URL only one
  // entry is added not matter the titles are same or not.
  [[nodiscard]] std::vector<UrlAndTitle> GetUniqueUrls() const;

  // Returns the type of `folder` as represented in metrics.
  metrics::BookmarkFolderTypeForUMA GetFolderType(
      const BookmarkNode* folder) const;

  // Adds a new folder node at the specified position with the given
  // `creation_time`, `uuid` and `meta_info`. If no UUID is provided (i.e.
  // nullopt), then a random one will be generated. If a UUID is provided, it
  // must be valid.
  const BookmarkNode* AddFolder(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const BookmarkNode::MetaInfoMap* meta_info = nullptr,
      absl::optional<base::Time> creation_time = absl::nullopt,
      absl::optional<base::Uuid> uuid = absl::nullopt);

  // Adds a new bookmark for the given `url` at the specified position with the
  // given `meta_info`. Used for bookmarks being added through some direct user
  // action (e.g. the bookmark star).
  const BookmarkNode* AddNewURL(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const BookmarkNode::MetaInfoMap* meta_info = nullptr);

  // Adds a url at the specified position with the given `creation_time`,
  // `meta_info`, `uuid`, and `last_used_time`. If no UUID is provided
  // (i.e. nullopt), then a random one will be generated. If a UUID is
  // provided, it must be valid. Used for bookmarks not being added from
  // direct user actions (e.g. created via sync, locally modified bookmark
  // or pre-existing bookmark). `added_by_user` is true when a new bookmark was
  // added by the user and false when a node is added by sync or duplicated.
  const BookmarkNode* AddURL(
      const BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url,
      const BookmarkNode::MetaInfoMap* meta_info = nullptr,
      absl::optional<base::Time> creation_time = absl::nullopt,
      absl::optional<base::Uuid> uuid = absl::nullopt,
      bool added_by_user = false);

  // Sorts the children of `parent`, notifying observers by way of the
  // BookmarkNodeChildrenReordered method.
  void SortChildren(const BookmarkNode* parent);

  // Order the children of `parent` as specified in `ordered_nodes`.  This
  // function should only be used to reorder the child nodes of `parent` and
  // is not meant to move nodes between different parent. Notifies observers
  // using the BookmarkNodeChildrenReordered method.
  void ReorderChildren(const BookmarkNode* parent,
                       const std::vector<const BookmarkNode*>& ordered_nodes);

  // Sets the date when the folder was modified.
  void SetDateFolderModified(const BookmarkNode* node, const base::Time time);

  // Resets the 'date modified' time of the node to 0. This is used during
  // importing to exclude the newly created folders from showing up in the
  // combobox of most recently modified folders.
  void ResetDateFolderModified(const BookmarkNode* node);

  // Updates the last used `time` for the given `node`. `just_opened`
  // indicates whether this is being called as a result of the bookmark being
  // opened. `just_opened` being false means that this update didn't come from
  // a user, such as sync updating a node automatically.
  void UpdateLastUsedTime(const BookmarkNode* node,
                          const base::Time time,
                          bool just_opened);

  // Clears the last used time for the given time range. Called when the user
  // clears their history. Time() and Time::Max() are used for min/max values.
  void ClearLastUsedTimeInRange(const base::Time delete_begin,
                                const base::Time delete_end);

  // Returns up to `max_count` bookmarks containing each term from `query` in
  // either the title, URL, or the titles of ancestors. `matching_algorithm`
  // determines the algorithm used by QueryParser internally to parse `query`.
  [[nodiscard]] std::vector<TitledUrlMatch> GetBookmarksMatching(
      const std::u16string& query,
      size_t max_count,
      query_parser::MatchingAlgorithm matching_algorithm) const;

  // Sets the store to NULL, making it so the BookmarkModel does not persist
  // any changes to disk. This is only useful during testing to speed up
  // testing.
  void ClearStore();

  // Returns the next node ID.
  int64_t next_node_id() const { return next_node_id_; }

  // Sets/deletes meta info of `node`.
  void SetNodeMetaInfo(const BookmarkNode* node,
                       const std::string& key,
                       const std::string& value);
  void SetNodeMetaInfoMap(const BookmarkNode* node,
                          const BookmarkNode::MetaInfoMap& meta_info_map);
  void DeleteNodeMetaInfo(const BookmarkNode* node, const std::string& key);

  // Notify BookmarkModel that the favicons for the given page URLs (e.g.
  // http://www.google.com) and the given icon URL (e.g.
  // http://www.google.com/favicon.ico) have changed. It is valid to call
  // OnFaviconsChanged() with non-empty `page_urls` and an empty `icon_url` and
  // vice versa.
  void OnFaviconsChanged(const std::set<GURL>& page_urls, const GURL& icon_url);

  // Returns the client used by this BookmarkModel.
  BookmarkClient* client() const { return client_.get(); }

  // Creates folders for account storage. Must be invoked by sync code only.
  // Must only be invoked after BookmarkModel is loaded.
  void CreateAccountPermanentFolders();

  // Removes folders for account storage. Calling this method will destroy ALL
  // bookmarks in account storage, including permanent folders. Must be invoked
  // by sync code only. Must only be invoked after BookmarkModel is loaded.
  void RemoveAccountPermanentFolders();

  base::WeakPtr<BookmarkModel> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Attempts to delete the account storage file in case the account storage
  // support was rolled back. If the account storage support wasn't enabled -
  // this is a no-op. Deletion is done asynchronously on a background thread.
  // TODO(crbug.com/1497923): Remove this method.
  static void WipeAccountStorageForRollback(const base::FilePath& profile_path);

  // Similar to Load() but allows unit-tests to mimic an empty JSON file being
  // loaded from disk, without dealing with actual files, and complete loading
  // synchronously.
  void LoadEmptyForTest();

  // TODO(crbug.com/1494120): Replace with an actual, non-test API.
  void CreateAccountPermanentFoldersForTest();

 private:
  friend class BookmarkCodecTest;
  friend class BookmarkModelFaviconTest;
  friend class BookmarkStorage;
  friend class ScopedGroupBookmarkActions;

  // BookmarkUndoProvider:
  void RestoreRemovedNode(const BookmarkNode* parent,
                          size_t index,
                          std::unique_ptr<BookmarkNode> node) override;

  // Notifies the observers for adding every descendant of `node`.
  void NotifyNodeAddedForAllDescendants(const BookmarkNode* node,
                                        bool added_by_user);

  // Removes the node from internal maps and recurses through all children. If
  // the node is a url, its url is added to removed_urls.
  //
  // This does NOT delete the node.
  void RemoveNodeFromIndicesRecursive(BookmarkNode* node);

  // Clones `node` and all its descendants (if any) for adding it in
  // `dest_model`. Doesn't add it to `dest_model` - this is the responsibility
  // of the caller. Bookmarks IDs are not copied and new IDs are generated
  // instead.
  std::unique_ptr<BookmarkNode> CloneSubtreeForOtherModelWithNewNodeIdsAndUuids(
      const BookmarkNode* node,
      BookmarkModel* dest_model);

  // Called when done loading. Updates internal state and notifies observers.
  void DoneLoading(std::unique_ptr<BookmarkLoadDetails> details);

  // Adds the `node` at `parent` in the specified `index` and notifies its
  // observers. `added_by_user` is true when a new bookmark was added by the
  // user and false when a node is added by sync or duplicated.
  BookmarkNode* AddNode(BookmarkNode* parent,
                        size_t index,
                        std::unique_ptr<BookmarkNode> node,
                        bool added_by_user = false);

  // Adds `node` to all lookups indices and recursively invokes this for all
  // children.
  void AddNodeToIndicesRecursive(const BookmarkNode* node);

  // Returns true if the parent and index are valid.
  bool IsValidIndex(const BookmarkNode* parent, size_t index, bool allow_end);

  // Notification that a favicon has finished loading. If we can decode the
  // favicon, FaviconLoaded is invoked.
  void OnFaviconDataAvailable(
      BookmarkNode* node,
      const favicon_base::FaviconImageResult& image_result);

  // Invoked from the node to load the favicon. Requests the favicon from the
  // favicon service.
  void LoadFavicon(BookmarkNode* node);

  // Called to notify the observers that the favicon has been loaded.
  void FaviconLoaded(const BookmarkNode* node);

  // If we're waiting on a favicon for node, the load request is canceled.
  void CancelPendingFaviconLoadRequests(BookmarkNode* node);

  // Notifies the observers that a set of changes initiated by a single user
  // action is about to happen and has completed.
  void BeginGroupedChanges();
  void EndGroupedChanges();

  // Generates and returns the next node ID.
  int64_t generate_next_node_id();

  // Sets the maximum node ID to the given value.
  // This is used by BookmarkCodec to report the maximum ID after it's done
  // decoding since during decoding codec assigns node IDs.
  void set_next_node_id(int64_t id) { next_node_id_ = id; }

  // Implementation of `UpdateLastUsedTime` which gives the option to skip
  // saving the change to `BookmarkStorage. Used to efficiently make changes
  // to multiple bookmarks.
  void UpdateLastUsedTimeImpl(const BookmarkNode* node, base::Time time);

  void ClearLastUsedTimeInRangeRecursive(BookmarkNode* node,
                                         const base::Time delete_begin,
                                         const base::Time delete_end);

  // Whether the initial set of data has been loaded.
  bool loaded_ = false;

  // See `root_` for details.
  std::unique_ptr<BookmarkNode> owned_root_;

  // The root node. This contains the bookmark bar node, the 'other' node and
  // the mobile node as children. The value of `root_` is initially that of
  // `owned_root_`. Once loading has completed, `owned_root_` is destroyed and
  // this is set to url_index_->root(). `owned_root_` is done as lots of
  // existing code assumes the root is non-null while loading.
  raw_ptr<BookmarkNode, AcrossTasksDanglingUntriaged> root_ = nullptr;

  raw_ptr<BookmarkPermanentNode, AcrossTasksDanglingUntriaged>
      bookmark_bar_node_ = nullptr;
  raw_ptr<BookmarkPermanentNode, AcrossTasksDanglingUntriaged> other_node_ =
      nullptr;
  raw_ptr<BookmarkPermanentNode, AcrossTasksDanglingUntriaged> mobile_node_ =
      nullptr;

  // Permanent nodes for account storage.
  raw_ptr<BookmarkPermanentNode> account_bookmark_bar_node_ = nullptr;
  raw_ptr<BookmarkPermanentNode> account_other_node_ = nullptr;
  raw_ptr<BookmarkPermanentNode> account_mobile_node_ = nullptr;

  // The maximum ID assigned to the bookmark nodes in the model.
  int64_t next_node_id_ = 1;

  // The observers.
#if BUILDFLAG(IS_IOS)
  // TODO(crbug.com/1470748) Set the parameter to `true` on all platforms.
  base::ObserverList<BookmarkModelObserver, true> observers_;
#else
  base::ObserverList<BookmarkModelObserver> observers_;
#endif

  std::unique_ptr<BookmarkClient> client_;

  // Used for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Writes bookmarks to disk.
  std::unique_ptr<BookmarkStorage> store_;

  std::unique_ptr<TitledUrlIndex> titled_url_index_;

  // All nodes indexed by UUID.
  UuidIndex uuid_index_;

  scoped_refptr<UrlIndex> url_index_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_ = 0;

  scoped_refptr<ModelLoader> model_loader_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BookmarkModel> weak_factory_{this};
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_
