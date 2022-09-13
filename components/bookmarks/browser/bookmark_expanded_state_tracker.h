// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_EXPANDED_STATE_TRACKER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_EXPANDED_STATE_TRACKER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"

class PrefService;

namespace bookmarks {

class BookmarkModel;
class BookmarkNode;

// BookmarkExpandedStateTracker is used to track a set of expanded nodes. The
// nodes are persisted in preferences. If an expanded node is removed from the
// model BookmarkExpandedStateTracker removes the node.
class BookmarkExpandedStateTracker : public BaseBookmarkModelObserver {
 public:
  typedef std::set<const BookmarkNode*> Nodes;

  BookmarkExpandedStateTracker(BookmarkModel* bookmark_model,
                               PrefService* pref_service);

  BookmarkExpandedStateTracker(const BookmarkExpandedStateTracker&) = delete;
  BookmarkExpandedStateTracker& operator=(const BookmarkExpandedStateTracker&) =
      delete;

  ~BookmarkExpandedStateTracker() override;

  // The set of expanded nodes.
  void SetExpandedNodes(const Nodes& nodes);
  Nodes GetExpandedNodes();

 private:
  // BaseBookmarkModelObserver:
  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override;
  void BookmarkModelChanged() override;
  void BookmarkModelBeingDeleted(BookmarkModel* model) override;
  void BookmarkNodeRemoved(BookmarkModel* model,
                           const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkAllUserNodesRemoved(BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;

  // Updates the value for |prefs::kBookmarkEditorExpandedNodes| from
  // GetExpandedNodes().
  void UpdatePrefs(const Nodes& nodes);

  raw_ptr<BookmarkModel> bookmark_model_;
  raw_ptr<PrefService> pref_service_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_EXPANDED_STATE_TRACKER_H_
