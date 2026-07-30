// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_VIEW_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_VIEW_H_

#include <optional>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "url/gurl.h"

namespace bookmarks_api {

class BookmarkEventTranslator;
class BookmarksViewObserver;

// Represents a hierarchical view of bookmark nodes and supports operations on
// that view. This interface allows injecting different view models (such as
// merged or interleaved surfaces) into the bookmarks API without coupling the
// API to specific UI services or models.
class BookmarksView {
 public:
  virtual ~BookmarksView() = default;

  // Observers.
  virtual void AddObserver(BookmarksViewObserver* observer) = 0;
  virtual void RemoveObserver(BookmarksViewObserver* observer) = 0;
  virtual bool IsDoingExtensiveChanges() const = 0;

  // Tree queries and lookups.
  virtual const bookmarks::BookmarkNode* GetRootNode() const = 0;
  virtual std::vector<const bookmarks::BookmarkNode*> GetChildren(
      const bookmarks::BookmarkNode* parent) const = 0;
  virtual std::optional<const bookmarks::BookmarkNode*> FindNodeByUuid(
      const base::Uuid& uuid) const = 0;
  virtual bool IsPermanentNode(const bookmarks::BookmarkNode* node) const = 0;
  virtual mojom::PermanentFolderType GetPermanentFolderType(
      const bookmarks::BookmarkNode* node) const = 0;
  virtual base::Uuid GetUuid(const bookmarks::BookmarkNode* node) const = 0;
  virtual bool IsSynced(const bookmarks::BookmarkNode* node) const = 0;
  virtual const BookmarkEventTranslator& GetEventTranslator() const = 0;

  // Hierarchical mutations (all indices are visual view indices).
  virtual const bookmarks::BookmarkNode* AddURL(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title,
      const GURL& url) = 0;
  virtual const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title) = 0;
  virtual void Move(const bookmarks::BookmarkNode* node,
                    const bookmarks::BookmarkNode* new_parent,
                    size_t index) = 0;
  virtual void SetTitle(const bookmarks::BookmarkNode* node,
                        const std::u16string& title,
                        bookmarks::metrics::BookmarkEditSource source) = 0;
  virtual void SetURL(const bookmarks::BookmarkNode* node,
                      const GURL& url,
                      bookmarks::metrics::BookmarkEditSource source) = 0;
  virtual void Remove(const bookmarks::BookmarkNode* node,
                      bookmarks::metrics::BookmarkEditSource source,
                      const base::Location& location) = 0;
  virtual void RemoveNodes(
      const std::vector<const bookmarks::BookmarkNode*>& nodes,
      bookmarks::metrics::BookmarkEditSource source,
      const base::Location& location) = 0;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARKS_VIEW_H_
