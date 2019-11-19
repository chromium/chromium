// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace bookmarks {

class BookmarkModel;

// BookmarkNode ---------------------------------------------------------------

// BookmarkNode contains information about a starred entry: title, URL, favicon,
// id and type. BookmarkNodes are returned from BookmarkModel.
class BookmarkNode : public ui::TreeNode<BookmarkNode>, public TitledUrlNode {
 public:
  enum Type {
    URL,
    FOLDER,
    BOOKMARK_BAR,
    OTHER_NODE,
    MOBILE
  };

  enum FaviconState {
    INVALID_FAVICON,
    LOADING_FAVICON,
    LOADED_FAVICON,
  };

  typedef std::map<std::string, std::string> MetaInfoMap;

  static const int64_t kInvalidSyncTransactionVersion;
  static const char kRootNodeGuid[];
  static const char kBookmarkBarNodeGuid[];
  static const char kOtherBookmarksNodeGuid[];
  static const char kMobileBookmarksNodeGuid[];
  static const char kManagedNodeGuid[];

  // Creates a new node with |id|, |guid| and |url|.
  BookmarkNode(int64_t id, const std::string& guid, const GURL& url);

  ~BookmarkNode() override;

  static std::string RootNodeGuid();

  // Returns true if the node is a BookmarkPermanentNode (which does not include
  // the root).
  bool is_permanent_node() const { return is_permanent_node_; }

  // Set the node's internal title. Note that this neither invokes observers
  // nor updates any bookmark model this node may be in. For that functionality,
  // BookmarkModel::SetTitle(..) should be used instead.
  void SetTitle(const base::string16& title) override;

  // Returns an unique id for this node.
  // For bookmark nodes that are managed by the bookmark model, the IDs are
  // persisted across sessions.
  int64_t id() const { return id_; }
  void set_id(int64_t id) { id_ = id; }

  // Returns the GUID for this node.
  // For bookmark nodes that are managed by the bookmark model, the GUIDs are
  // persisted across sessions and stable throughout the lifetime of the
  // bookmark.
  const std::string& guid() const { return guid_; }

  const GURL& url() const { return url_; }
  void set_url(const GURL& url) { url_ = url; }

  // Returns the favicon's URL. Returns an empty URL if there is no favicon
  // associated with this bookmark.
  const GURL* icon_url() const { return icon_url_ ? icon_url_.get() : nullptr; }

  Type type() const { return type_; }

  // Returns the time the node was added.
  const base::Time& date_added() const { return date_added_; }
  void set_date_added(const base::Time& date) { date_added_ = date; }

  // Returns the last time the folder was modified. This is only maintained
  // for folders (including the bookmark bar and other folder).
  const base::Time& date_folder_modified() const {
    return date_folder_modified_;
  }
  void set_date_folder_modified(const base::Time& date) {
    date_folder_modified_ = date;
  }

  // Convenience for testing if this node represents a folder. A folder is a
  // node whose type is not URL.
  bool is_folder() const { return type_ != URL; }
  bool is_url() const { return type_ == URL; }

  bool is_favicon_loaded() const { return favicon_state_ == LOADED_FAVICON; }

  // Accessor method for controlling the visibility of a bookmark node/sub-tree.
  // Note that visibility is not propagated down the tree hierarchy so if a
  // parent node is marked as invisible, a child node may return "Visible". This
  // function is primarily useful when traversing the model to generate a UI
  // representation but we may want to suppress some nodes.
  virtual bool IsVisible() const;

  // Gets/sets/deletes value of |key| in the meta info represented by
  // |meta_info_str_|. Return true if key is found in meta info for gets or
  // meta info is changed indeed for sets/deletes.
  bool GetMetaInfo(const std::string& key, std::string* value) const;
  bool SetMetaInfo(const std::string& key, const std::string& value);
  bool DeleteMetaInfo(const std::string& key);
  void SetMetaInfoMap(const MetaInfoMap& meta_info_map);
  // Returns NULL if there are no values in the map.
  const MetaInfoMap* GetMetaInfoMap() const;

  void set_sync_transaction_version(int64_t sync_transaction_version) {
    sync_transaction_version_ = sync_transaction_version;
  }
  int64_t sync_transaction_version() const { return sync_transaction_version_; }

  // TitledUrlNode interface methods.
  const base::string16& GetTitledUrlNodeTitle() const override;
  const GURL& GetTitledUrlNodeUrl() const override;

  // TODO(sky): Consider adding last visit time here, it'll greatly simplify
  // HistoryContentsProvider.

 protected:
  BookmarkNode(int64_t id,
               const std::string& guid,
               const GURL& url,
               Type type,
               bool is_permanent_node);

 private:
  friend class BookmarkModel;

  // Called when the favicon becomes invalid.
  void InvalidateFavicon();

  // Sets the favicon's URL.
  void set_icon_url(const GURL& icon_url) {
    icon_url_ = std::make_unique<GURL>(icon_url);
  }

  // Returns the favicon. In nearly all cases you should use the method
  // BookmarkModel::GetFavicon rather than this one as it takes care of
  // loading the favicon if it isn't already loaded.
  const gfx::Image& favicon() const { return favicon_; }
  void set_favicon(const gfx::Image& icon) { favicon_ = icon; }

  favicon_base::IconType favicon_type() const { return favicon_type_; }
  void set_favicon_type(favicon_base::IconType type) { favicon_type_ = type; }

  FaviconState favicon_state() const { return favicon_state_; }
  void set_favicon_state(FaviconState state) { favicon_state_ = state; }

  base::CancelableTaskTracker::TaskId favicon_load_task_id() const {
    return favicon_load_task_id_;
  }
  void set_favicon_load_task_id(base::CancelableTaskTracker::TaskId id) {
    favicon_load_task_id_ = id;
  }

  // The unique identifier for this node.
  int64_t id_;

  // The GUID for this node. A BookmarkNode GUID is immutable and differs from
  // the |id_| in that it is consistent across different clients and
  // stable throughout the lifetime of the bookmark, with the exception of nodes
  // added to the Managed Bookmarks folder, whose GUIDs are re-assigned at
  // start-up every time.
  const std::string guid_;

  // The URL of this node. BookmarkModel maintains maps off this URL, so changes
  // to the URL must be done through the BookmarkModel.
  GURL url_;

  // The type of this node. See enum above.
  const Type type_;

  // Date of when this node was created.
  base::Time date_added_;

  // Date of the last modification. Only used for folders.
  base::Time date_folder_modified_;

  // The favicon of this node.
  gfx::Image favicon_;

  // The type of favicon currently loaded.
  favicon_base::IconType favicon_type_;

  // The URL of the node's favicon.
  std::unique_ptr<GURL> icon_url_;

  // The loading state of the favicon.
  FaviconState favicon_state_ = INVALID_FAVICON;

  // If not base::CancelableTaskTracker::kBadTaskId, it indicates
  // we're loading the
  // favicon and the task is tracked by CancelabelTaskTracker.
  base::CancelableTaskTracker::TaskId favicon_load_task_id_ =
      base::CancelableTaskTracker::kBadTaskId;

  // A map that stores arbitrary meta information about the node.
  std::unique_ptr<MetaInfoMap> meta_info_map_;

  // The sync transaction version.
  int64_t sync_transaction_version_ = kInvalidSyncTransactionVersion;

  const bool is_permanent_node_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNode);
};

// BookmarkPermanentNode -------------------------------------------------------

// Node used for the permanent folders (excluding the root).
class BookmarkPermanentNode : public BookmarkNode {
 public:
  BookmarkPermanentNode(int64_t id, Type type);
  ~BookmarkPermanentNode() override;

  // WARNING: this code is used for other projects. Contact noyau@ for details.
  void set_visible(bool value) { visible_ = value; }

  // BookmarkNode overrides:
  bool IsVisible() const override;

 private:
  bool visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(BookmarkPermanentNode);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_H_
