// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/titled_url_node.h"
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

  typedef base::flat_map<std::string, std::string> MetaInfoMap;

  BookmarkNode(int64_t id, const base::Uuid& uuid, const GURL& url);

  BookmarkNode(const BookmarkNode&) = delete;
  BookmarkNode& operator=(const BookmarkNode&) = delete;

  ~BookmarkNode() override;

  // Returns true if the node is a BookmarkPermanentNode (which does not include
  // the root).
  bool is_permanent_node() const { return is_permanent_node_; }

  // Set the node's internal title. Note that this neither invokes observers
  // nor updates any bookmark model this node may be in. For that functionality,
  // BookmarkModel::SetTitle(..) should be used instead.
  void SetTitle(const std::u16string& title) override;

  // Returns an unique id for this node.
  // For bookmark nodes that are managed by the bookmark model, the IDs are
  // persisted across sessions.
  int64_t id() const { return id_; }
  void set_id(int64_t id) { id_ = id; }

  // Returns this node's UUID, which is guaranteed to be valid.
  // For bookmark nodes that are managed by the bookmark model, the UUIDs are
  // persisted across sessions and stable throughout the lifetime of the
  // bookmark, with the exception of rare cases where moving a bookmark would
  // otherwise produce a UUID collision (when moved from local to account or
  // the other way round).
  const base::Uuid& uuid() const { return uuid_; }

  const GURL& url() const { return url_; }
  void set_url(const GURL& url) { url_ = url; }

  // Returns the favicon's URL. Return null if there is no favicon associated
  // with this bookmark.
  const GURL* icon_url() const { return icon_url_.get(); }

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
  bool is_favicon_loading() const { return favicon_state_ == LOADING_FAVICON; }

  // Accessor method for controlling the visibility of a bookmark node/sub-tree.
  // Note that visibility is not propagated down the tree hierarchy so if a
  // parent node is marked as invisible, a child node may return "Visible". This
  // function is primarily useful when traversing the model to generate a UI
  // representation but we may want to suppress some nodes.
  virtual bool IsVisible() const;

  // Gets/sets/deletes value of `key` in the meta info represented by
  // `meta_info_str_`. Return true if key is found in meta info for gets or
  // meta info is changed indeed for sets/deletes.
  bool GetMetaInfo(const std::string& key, std::string* value) const;
  bool SetMetaInfo(const std::string& key, const std::string& value);
  bool DeleteMetaInfo(const std::string& key);
  void SetMetaInfoMap(const MetaInfoMap& meta_info_map);
  // Returns NULL if there are no values in the map.
  const MetaInfoMap* GetMetaInfoMap() const;

  // TitledUrlNode interface methods.
  const std::u16string& GetTitledUrlNodeTitle() const override;
  const GURL& GetTitledUrlNodeUrl() const override;
  std::vector<std::u16string_view> GetTitledUrlNodeAncestorTitles()
      const override;

  // Returns the last time the bookmark was opened. This is only maintained
  // for urls (no folders).
  base::Time date_last_used() const { return date_last_used_; }
  void set_date_last_used(const base::Time date_last_used) {
    date_last_used_ = date_last_used;
  }

 protected:
  BookmarkNode(int64_t id,
               const base::Uuid& uuid,
               const GURL& url,
               Type type,
               bool is_permanent_node);

 private:
  friend class BookmarkModel;

  // Reassignment of UUIDs, used to avoid UUID collisions when a bookmark is
  // moved.
  void SetNewRandomUuid() { uuid_ = base::Uuid::GenerateRandomV4(); }

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

  // The UUID for this node. A BookmarkNode UUID is generally immutable (barring
  // advanced scenarios) and differs from the `id_` in that it is consistent
  // across different clients. For managed bookmarks, the UUID is not actually
  // stable and UUIDs are re-assigned at start-up every time.
  base::Uuid uuid_;

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

  // The URL of the node's favicon.
  std::unique_ptr<GURL> icon_url_;

  // The loading state of the favicon.
  FaviconState favicon_state_ = INVALID_FAVICON;

  // If not base::CancelableTaskTracker::kBadTaskId, it indicates
  // we're loading the
  // favicon and the task is tracked by CancelableTaskTracker.
  base::CancelableTaskTracker::TaskId favicon_load_task_id_ =
      base::CancelableTaskTracker::kBadTaskId;

  // A map that stores arbitrary meta information about the node.
  std::unique_ptr<MetaInfoMap> meta_info_map_;

  const bool is_permanent_node_;

  base::Time date_last_used_;
};

// BookmarkPermanentNode -------------------------------------------------------

// Node used for the permanent folders (excluding the root).
class BookmarkPermanentNode : public BookmarkNode {
 public:
  // Permanent nodes are well-known, it's not allowed to create arbitrary ones.
  static std::unique_ptr<BookmarkPermanentNode> CreateManagedBookmarks(
      int64_t id);

  // Permanent nodes are well-known, it's not allowed to create arbitrary ones.
  // Note that the same UUID is used for local-or-syncable instances and
  // account permanent folders (as exposed by BookmarkModel APIs).
  static std::unique_ptr<BookmarkPermanentNode> CreateBookmarkBar(int64_t id);
  static std::unique_ptr<BookmarkPermanentNode> CreateOtherBookmarks(
      int64_t id);
  static std::unique_ptr<BookmarkPermanentNode> CreateMobileBookmarks(
      int64_t id);

  // Returns whether the permanent node of type `type` should be visible even
  // when it is empty (i.e. no children).
  static bool IsTypeVisibleWhenEmpty(Type type);

  BookmarkPermanentNode(const BookmarkPermanentNode&) = delete;
  BookmarkPermanentNode& operator=(const BookmarkPermanentNode&) = delete;

  ~BookmarkPermanentNode() override;

  // BookmarkNode overrides:
  bool IsVisible() const override;

 private:
  // Constructor is private to disallow the construction of permanent nodes
  // other than the well-known ones, see factory methods.
  BookmarkPermanentNode(int64_t id,
                        Type type,
                        const base::Uuid& uuid,
                        const std::u16string& title);

  const bool visible_when_empty_;
};

// If you are looking for gMock printing via PrintTo(), please check
// bookmark_test_util.h.

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_H_
