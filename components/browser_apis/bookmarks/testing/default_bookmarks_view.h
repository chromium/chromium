// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_TESTING_DEFAULT_BOOKMARKS_VIEW_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_TESTING_DEFAULT_BOOKMARKS_VIEW_H_

#include <optional>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/browser_apis/bookmarks/bookmarks_view.h"
#include "url/gurl.h"

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

namespace bookmarks_api {

// Default implementation that directly forwards operations to BookmarkModel.
// This is intended for use in tests where a merged view is not needed.
class DefaultBookmarksView : public BookmarksView {
 public:
  explicit DefaultBookmarksView(
      bookmarks::BookmarkModel* model,
      bookmarks::ManagedBookmarkService* managed_service = nullptr);
  ~DefaultBookmarksView() override;

  DefaultBookmarksView(const DefaultBookmarksView&) = delete;
  DefaultBookmarksView& operator=(const DefaultBookmarksView&) = delete;

  // BookmarksView:
  void AddObserver(bookmarks::BookmarkModelObserver* observer) override;
  void RemoveObserver(bookmarks::BookmarkModelObserver* observer) override;
  bool IsDoingExtensiveChanges() const override;
  const bookmarks::BookmarkNode* GetRootNode() const override;
  std::vector<const bookmarks::BookmarkNode*> GetChildren(
      const bookmarks::BookmarkNode* parent) const override;
  std::optional<const bookmarks::BookmarkNode*> FindNodeByUuid(
      const base::Uuid& uuid) const override;
  bool IsPermanentNode(const bookmarks::BookmarkNode* node) const override;
  mojom::PermanentFolderType GetPermanentFolderType(
      const bookmarks::BookmarkNode* node) const override;
  bool IsSynced(const bookmarks::BookmarkNode* node) const override;
  const bookmarks::BookmarkNode* AddURL(const bookmarks::BookmarkNode* parent,
                                        size_t index,
                                        const std::u16string& title,
                                        const GURL& url) override;
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title) override;
  void Move(const bookmarks::BookmarkNode* node,
            const bookmarks::BookmarkNode* new_parent,
            size_t index) override;
  void SetTitle(const bookmarks::BookmarkNode* node,
                const std::u16string& title,
                bookmarks::metrics::BookmarkEditSource source) override;
  void SetURL(const bookmarks::BookmarkNode* node,
              const GURL& url,
              bookmarks::metrics::BookmarkEditSource source) override;
  void Remove(const bookmarks::BookmarkNode* node,
              bookmarks::metrics::BookmarkEditSource source,
              const base::Location& location) override;
  void RemoveNodes(const std::vector<const bookmarks::BookmarkNode*>& nodes,
                   bookmarks::metrics::BookmarkEditSource source,
                   const base::Location& location) override;

 private:
  raw_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_service_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_TESTING_DEFAULT_BOOKMARKS_VIEW_H_
