// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_OBSERVER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_OBSERVER_H_

#include <stddef.h>

#include <set>

#include "base/location.h"
#include "base/observer_list_types.h"

class GURL;

namespace bookmarks {

class BookmarkNode;

// Observer for the BookmarkModel.
class BookmarkModelObserver : public base::CheckedObserver {
 public:
  BookmarkModelObserver(const BookmarkModelObserver&) = delete;
  BookmarkModelObserver& operator=(const BookmarkModelObserver&) = delete;

  // Invoked when the model has finished loading. `ids_reassigned` mirrors
  // that of BookmarkLoadDetails::ids_reassigned. See it for details.
  virtual void BookmarkModelLoaded(bool ids_reassigned) = 0;

  // Invoked from the destructor of the BookmarkModel.
  virtual void BookmarkModelBeingDeleted() {}

  // Invoked when a node has moved.
  virtual void BookmarkNodeMoved(const BookmarkNode* old_parent,
                                 size_t old_index,
                                 const BookmarkNode* new_parent,
                                 size_t new_index) = 0;

  // Invoked when a node has been added. If the added node has any descendants,
  // BookmarkModel` will invoke `BookmarkNodeAdded` recursively for all these
  // descendants.
  //
  // `added_by_user` is true when a new bookmark was added by the user and false
  // when a node is added by sync or duplicated.
  virtual void BookmarkNodeAdded(const BookmarkNode* parent,
                                 size_t index,
                                 bool added_by_user) = 0;

  // Invoked prior to removing a node from the model. When a node is removed
  // it's descendants are implicitly removed from the model as
  // well. Notification is only sent for the node itself, not any
  // descendants. For example, if folder 'A' has the children 'A1' and 'A2',
  // model->Remove('A') generates a single notification for 'A'; no notification
  // is sent for 'A1' or 'A2'.
  //
  // `parent` the parent of the node that will be removed.
  // `old_index` the index of the node about to be removed in `parent`.
  // `node` is the node to be removed.
  virtual void OnWillRemoveBookmarks(const BookmarkNode* parent,
                                     size_t old_index,
                                     const BookmarkNode* node,
                                     const base::Location& location) {}

  // Invoked after a node has been removed from the model. Removing a node
  // implicitly removes all descendants. Notification is only sent for the node
  // that BookmarkModel::Remove() is invoked on. See description of
  // OnWillRemoveBookmarks() for details.
  //
  // `parent` the parent of the node that was removed.
  // `old_index` the index of the removed node in `parent` before it was
  // removed.
  // `node` the node that was removed.
  // `no_longer_bookmarked` contains the urls of any nodes that are no longer
  // bookmarked as a result of the removal.
  virtual void BookmarkNodeRemoved(const BookmarkNode* parent,
                                   size_t old_index,
                                   const BookmarkNode* node,
                                   const std::set<GURL>& no_longer_bookmarked,
                                   const base::Location& location) = 0;

  // Invoked before the title or url of a node is changed. Subsequent
  // BookmarkNodeChanged call guaranteed to contain the same BookmarkNode.
  virtual void OnWillChangeBookmarkNode(const BookmarkNode* node) {}

  // Invoked when the title or url of a node changes. Guaranteed to contain the
  // same BookmarkNode as the preceding OnWillChangeBookmark Node call.
  virtual void BookmarkNodeChanged(const BookmarkNode* node) = 0;

  // Invoked before the metainfo of a node is changed.
  virtual void OnWillChangeBookmarkMetaInfo(const BookmarkNode* node) {}

  // Invoked when the metainfo on a node changes.
  virtual void BookmarkMetaInfoChanged(const BookmarkNode* node) {}

  // Invoked when a favicon has been loaded or changed.
  virtual void BookmarkNodeFaviconChanged(const BookmarkNode* node) = 0;

  // Invoked before the direct children of `node` have been reordered in some
  // way, such as sorted.
  virtual void OnWillReorderBookmarkNode(const BookmarkNode* node) {}

  // Invoked when the children (just direct children, not descendants) of
  // `node` have been reordered in some way, such as sorted.
  virtual void BookmarkNodeChildrenReordered(const BookmarkNode* node) = 0;

  // Invoked before an extensive set of model changes is about to begin.
  // This tells UI intensive observers to wait until the updates finish to
  // update themselves.
  // These methods should only be used for imports and sync.
  // Observers should still respond to BookmarkNodeRemoved immediately,
  // to avoid holding onto stale node pointers.
  virtual void ExtensiveBookmarkChangesBeginning() {}

  // Invoked after an extensive set of model changes has ended.
  // This tells observers to update themselves if they were waiting for the
  // update to finish.
  virtual void ExtensiveBookmarkChangesEnded() {}

  // Invoked before all non-permanent bookmark nodes that are editable by
  // the user are removed.
  virtual void OnWillRemoveAllUserBookmarks(const base::Location& location) {}

  // Invoked when all non-permanent bookmark nodes that are editable by the
  // user have been removed.
  // `removed_urls` is populated with the urls which no longer have any
  // bookmarks associated with them.
  virtual void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                           const base::Location& location) = 0;

  // Invoked before a set of model changes that is initiated by a single user
  // action. For example, this is called a single time when pasting from the
  // clipboard before each pasted bookmark is added to the bookmark model.
  virtual void GroupedBookmarkChangesBeginning() {}

  // Invoked after a set of model changes triggered by a single user action has
  // ended.
  virtual void GroupedBookmarkChangesEnded() {}

 protected:
  BookmarkModelObserver() = default;
  ~BookmarkModelObserver() override = default;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_MODEL_OBSERVER_H_
