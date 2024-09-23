// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_undo_provider.h"
#include "components/bookmarks/browser/uuid_index.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/keyed_service/core/keyed_service.h"
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
class BookmarkModel : public BookmarkUndoProvider,
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
  // BookmarkModelLoaded().
  void Load(const base::FilePath& profile_path);

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

  // Returns true if `node` represents a bookmark that is stored on the local
  // profile but not saved to the user's server-side account. The opposite case,
  // returning null, can happen because the user turned sync-the-feature on,
  // which syncs all bookmarks, or because `node` is a descendant of an account
  // permanent folder, e.g. `account_bookmark_bar_node()`.
  bool IsLocalOnlyNode(const BookmarkNode& node) const;

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
  // `source`. `location` is used for logging purposes and investigations.
  void Remove(const BookmarkNode* node,
              metrics::BookmarkEditSource source,
              const base::Location& location);

  // Removes all the non-permanent bookmark nodes that are editable by the user.
  // Observers are only notified when all nodes have been removed. There is no
  // notification for individual node removals. `location` is used for logging
  // purposes and investigations.
  void RemoveAllUserBookmarks(const base::Location& location);

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
  [[nodiscard]] std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
  GetNodesByURL(const GURL& url) const;

  // Enum determining a subset of bookmark nodes within a BookmarkModel for the
  // purpose of issuing UUID-based lookups. It is needed because, in some
  // advanced scenarios, the same UUID may be used by two BookmarkNode-s, in
  // particular if a bookmark duplicate exists as a result of Sync having left
  // behind a copy of the data.
  //
  // Below some guidance about the use of this enum:
  // 1. For callers that don't particularly mind the nature of the bookmark
  //    node, a sensible default is to issue two lookups, starting with
  //    `kAccountNodes` and (if the first returns null) following up with
  //    `kLocalOrSyncableNodes`.
  //
  // 2. For callers that specifically need to identify bookmarks that are saved
  //    (sync-ed) to the user's server-side account, it might be required to
  //    only conditionally issue the second lookup (`kLocalOrSyncableNodes`),
  //    based on whether user-facing Sync is on (exposed outside
  //    components/bookmarks, namely in SyncService).
  //
  // 3. For callers that specifically need the opposite, that is, identify
  //    bookmarks that are local-only, a lookup using `kLocalOrSyncableNodes`
  //    is sufficient, but it should also be done conditionally to the
  //    user-facing Sync being off (exposed outside components/bookmarks, namely
  //    in SyncService).
  //
  // In doubt, please reach out to components/bookmarks owners for guidance.
  enum class NodeTypeForUuidLookup {
    // Local or syncable nodes include all bookmark nodes that are not
    // descendants of account permanent folders (e.g. as returned by
    // account_bookmark_bar_node()).
    kLocalOrSyncableNodes,
    // Account nodes include all bookmarks that are descendants of account
    // permanent folders (e.g. as returned by account_bookmark_bar_node()).
    kAccountNodes,
  };

  // Returns the node with the given `uuid` among the subset of nodes determined
  // by `type`. Returns null if no node exists matching `uuid`.
  //
  // WARNING: UUID-based lookups are subtle and should be done with care, please
  // see details above in `NodeTypeForUuidLookup` and in doubt please reach out
  // to components/bookmarks owners for guidance.
  const BookmarkNode* GetNodeByUuid(const base::Uuid& uuid,
                                    NodeTypeForUuidLookup type) const;

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
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt);

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
      std::optional<base::Time> creation_time = std::nullopt,
      std::optional<base::Uuid> uuid = std::nullopt,
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

  // Disables the persistence to disk, useful during testing to speed up
  // testing.
  void DisableWritesToDiskForTest();

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

  // Similar to Load() but allows unit-tests to mimic an empty JSON file being
  // loaded from disk, without dealing with actual files, and complete loading
  // synchronously.
  void LoadEmptyForTest();

  // If a write to disk is scheduled, performs it immediately. This is useful
  // for tests that restart the browser without necessarily shutting it down
  // cleanly first.
  void CommitPendingWriteForTest();

  // Returns whether pending writes are pending/scheduled.
  bool LocalOrSyncableStorageHasPendingWriteForTest() const;
  bool AccountStorageHasPendingWriteForTest() const;

 private:
  friend class BookmarkCodecTest;
  friend class BookmarkModelFaviconTest;
  friend class BookmarkStorage;
  friend class ScopedGroupBookmarkActions;

  // BookmarkUndoProvider:
  void RestoreRemovedNode(const BookmarkNode* parent,
                          size_t index,
                          std::unique_ptr<BookmarkNode> node) override;

  // Given a node that is already part of the model, it determines the
  // corresponding type for the purpose of understanding uniqueness properties
  // of its UUID. That is, which subset of nodes this UUID is guaranteed to be
  // unique among.
  NodeTypeForUuidLookup DetermineTypeForUuidLookupForExistingNode(
      const BookmarkNode* node) const;

  // Notifies the observers for adding every descendant of `node`.
  void NotifyNodeAddedForAllDescendants(const BookmarkNode* node,
                                        bool added_by_user);

  // Called when done loading. Updates internal state and notifies observers.
  void DoneLoading(std::unique_ptr<BookmarkLoadDetails> details);

  // Adds the `node` at `parent` in the specified `index` and notifies its
  // observers. `added_by_user` is true when a new bookmark was added by the
  // user and false when a node is added by sync or duplicated.
  BookmarkNode* AddNode(BookmarkNode* parent,
                        size_t index,
                        std::unique_ptr<BookmarkNode> node,
                        bool added_by_user,
                        NodeTypeForUuidLookup type_for_uuid_lookup);

  // Adds `node` to all lookups indices and recursively invokes this for all
  // children.
  void AddNodeToIndicesRecursive(const BookmarkNode* node,
                                 NodeTypeForUuidLookup type_for_uuid_lookup);

  // Removes `node` and notifies its observers, returning and transferring
  // ownership of the node removed. The caller is responsible for allowing undo,
  // if applicable.
  std::unique_ptr<BookmarkNode> RemoveNode(const BookmarkNode* node,
                                           const base::Location& location);

  // Removes the node from internal maps and recurses through all children. If
  // the node is a url, its url is added to removed_urls.
  //
  // This does NOT delete the node.
  void RemoveNodeFromIndicesRecursive(
      BookmarkNode* node,
      NodeTypeForUuidLookup type_for_uuid_lookup);

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

  // Schedules saving the bookmark model to disk as a result of `node` having
  // changed. When multiple BookmarkStorage instances are involved, `node`
  // allows determining which of the two needs to be persisted.
  void ScheduleSaveForNode(const BookmarkNode* node);

  // Returns which BookmakStorage instance is used to persist `node` to disk.
  // The returned value will be either `local_or_syncable_store_` or
  // `account_store_`. It may return null in tests.
  BookmarkStorage* GetStorageForNode(const BookmarkNode* node);

  // Returns an enum representing how metrics associated to `node` should be
  // suffixed with for the purpose of metric breakdowns.
  metrics::StorageStateForUma GetStorageStateForUma(
      const BookmarkNode* node) const;

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
  // TODO(crbug.com/40277960) Set the parameter to `true` on all platforms.
  base::ObserverList<BookmarkModelObserver, true> observers_;
#else
  base::ObserverList<BookmarkModelObserver> observers_;
#endif

  std::unique_ptr<BookmarkClient> client_;

  // Used for loading favicons.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Write bookmarks to disk.
  std::unique_ptr<BookmarkStorage> local_or_syncable_store_;
  std::unique_ptr<BookmarkStorage> account_store_;

  std::unique_ptr<TitledUrlIndex> titled_url_index_;

  // All nodes indexed by UUID. An independent index exists for each value in
  // NodeTypeForUuidLookup, because UUID uniqueness is guaranteed only within
  // the scope of each NodeTypeForUuidLookup value.
  base::flat_map<NodeTypeForUuidLookup, UuidIndex> uuid_index_;

  scoped_refptr<UrlIndex> url_index_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_ = 0;

  scoped_refptr<ModelLoader> model_loader_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BookmarkModel> weak_factory_{this};
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_H_
