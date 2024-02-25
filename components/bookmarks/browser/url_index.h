// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_URL_INDEX_H_
#define COMPONENTS_BOOKMARKS_BROWSER_URL_INDEX_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/history_bookmark_model.h"
#include "url/gurl.h"

namespace bookmarks {

class BookmarkNode;

struct UrlLoadStats;
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

  UrlIndex(const UrlIndex&) = delete;
  UrlIndex& operator=(const UrlIndex&) = delete;

  BookmarkNode* root() { return root_.get(); }

  // Adds |node| to |parent| at |index|.
  void Add(BookmarkNode* parent,
           size_t index,
           std::unique_ptr<BookmarkNode> node);

  // Removes |node| and all its descendants from the map, adds urls that are no
  // longer contained in the index to the |removed_urls| set if provided
  // (doesn't clean up existing items in the set).
  std::unique_ptr<BookmarkNode> Remove(BookmarkNode* node,
                                       std::set<GURL>* removed_urls);

  // Mutation of bookmark node fields that are exposed to HistoryBookmarkModel,
  // which means must acquire a lock. Must be called from the UI thread.
  void SetUrl(BookmarkNode* node, const GURL& url);
  void SetTitle(BookmarkNode* node, const std::u16string& title);

  // Returns the nodes whose icon_url is |icon_url|.
  void GetNodesWithIconUrl(const GURL& icon_url,
                           std::set<const BookmarkNode*>* nodes);

  void GetNodesByUrl(
      const GURL& url,
      std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>* nodes);

  // Returns true if there is at least one bookmark.
  bool HasBookmarks() const;

  // Compute stats from the load.
  UrlLoadStats ComputeStats() const;

  // HistoryBookmarkModel:
  bool IsBookmarked(const GURL& url) override;
  [[nodiscard]] std::vector<UrlAndTitle> GetUniqueUrls() override;

 private:
  friend class base::RefCountedThreadSafe<UrlIndex>;

  ~UrlIndex() override;

  // Used to order BookmarkNodes by URL as well as lookups using GURL.
  class NodeUrlComparator {
   public:
    // Required by std::set to support GURL-based lookups.
    using is_transparent = void;

    bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) const {
      return n1->url() < n2->url();
    }
    bool operator()(const BookmarkNode* n1, const GURL& url2) const {
      return n1->url() < url2;
    }
    bool operator()(const GURL& url1, const BookmarkNode* n2) const {
      return url1 < n2->url();
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
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_URL_INDEX_H_
