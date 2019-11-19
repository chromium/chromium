// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_URL_INDEX_H_
#define COMPONENTS_BOOKMARKS_BROWSER_URL_INDEX_H_

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/history_bookmark_model.h"
#include "url/gurl.h"

namespace bookmarks {

class BookmarkNode;

struct UrlAndTitle;

// UrlIndex maintains the bookmark nodes of type url. The nodes are ordered by
// url for lookup using the url. The mapping is done based on the nodes in the
// model. This class may outlive the BookmarkModel, necessitating this class
// owning all the nodes.
//
// This class is accessed on multiple threads, so all mutation to the underlying
// set must be done while holding a lock. While this class is accessed on
// multiple threads, all mutation happens on main thread.
//
// This class is an implementation detail of BookmarkModel and is not intended
// to be public. The public functions implemented by way of
// HistoryBookmarkModel define the public API of this class.
class UrlIndex : public HistoryBookmarkModel {
 public:
  explicit UrlIndex(std::unique_ptr<BookmarkNode> root);

  BookmarkNode* root() { return root_.get(); }

  // Adds |node| to |parent| at |index|.
  void Add(BookmarkNode* parent,
           size_t index,
           std::unique_ptr<BookmarkNode> node);

  // Removes |node| and all its descendants from the map, returns the set of
  // urls that are no longer contained in the index.
  std::unique_ptr<BookmarkNode> Remove(BookmarkNode* node,
                                       std::set<GURL>* removed_urls);

  void SetUrl(BookmarkNode* node, const GURL& url);

  // Returns the nodes whose icon_url is |icon_url|.
  void GetNodesWithIconUrl(const GURL& icon_url,
                           std::set<const BookmarkNode*>* nodes);

  void GetNodesByUrl(const GURL& url, std::vector<const BookmarkNode*>* nodes);

  // Returns true if there is at least one bookmark.
  bool HasBookmarks() const;

  // Returns the number of URL bookmarks stored.
  size_t UrlCount() const;

  // HistoryBookmarkModel:
  bool IsBookmarked(const GURL& url) override;
  void GetBookmarks(std::vector<UrlAndTitle>* bookmarks) override;

 private:
  friend class base::RefCountedThreadSafe<UrlIndex>;

  ~UrlIndex() override;

  // Used to order BookmarkNodes by URL.
  class NodeUrlComparator {
   public:
    bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) const {
      return n1->url() < n2->url();
    }
  };

  bool IsBookmarkedNoLock(const GURL& url);

  void AddImpl(BookmarkNode* node);
  void RemoveImpl(BookmarkNode* node, std::set<GURL>* removed_urls);

  std::unique_ptr<BookmarkNode> root_;

  // Set of nodes ordered by URL. This is not a map to avoid copying the
  // urls.
  // WARNING: |nodes_ordered_by_url_set_| is accessed on multiple threads. As
  // such, be sure and wrap all usage of it around |url_lock_|.
  using NodesOrderedByUrlSet = std::multiset<BookmarkNode*, NodeUrlComparator>;
  NodesOrderedByUrlSet nodes_ordered_by_url_set_;
  mutable base::Lock url_lock_;

  DISALLOW_COPY_AND_ASSIGN(UrlIndex);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_URL_INDEX_H_
