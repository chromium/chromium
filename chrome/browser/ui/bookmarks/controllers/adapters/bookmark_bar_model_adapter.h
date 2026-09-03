// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_MODEL_ADAPTER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_MODEL_ADAPTER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/bookmarks/bookmark_node_types.h"

struct BookmarkParentFolder;

namespace bookmarks {
class BookmarkNode;
}

class BookmarkBarModelAdapter {
 public:
  virtual ~BookmarkBarModelAdapter() = default;

  // Returns true if the bookmark models and services are loaded.
  virtual bool IsLoaded() const = 0;

  // Returns the bookmark node with `id`, or nullptr if not found.
  virtual const bookmarks::BookmarkNode* GetNodeById(int64_t id) const = 0;

  // Returns the underlying bookmark nodes for a folder (permanent folder or
  // non-permanent folder node).
  virtual std::vector<const bookmarks::BookmarkNode*> GetUnderlyingNodes(
      const bookmarks::BookmarkNodeId& folder) const = 0;

  // Asynchronously checks whether bookmarks can be pasted from the clipboard
  // into `parent`.
  virtual void CanPasteFromClipboard(
      const BookmarkParentFolder* parent,
      base::OnceCallback<void(bool)> callback) = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ADAPTERS_BOOKMARK_BAR_MODEL_ADAPTER_H_
