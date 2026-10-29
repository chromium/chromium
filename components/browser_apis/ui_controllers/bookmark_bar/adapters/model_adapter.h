// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_MODEL_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_MODEL_ADAPTER_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom-forward.h"

namespace bookmarks {
class BookmarkNode;
}

namespace bookmark_bar_api {

// Gives the controller access to the embedder's bookmark model. Bookmarks must
// be loaded before any of these are called.
class ModelAdapter {
 public:
  virtual ~ModelAdapter() = default;

  // Returns the bookmark node with `id`, or nullptr if not found.
  virtual const bookmarks::BookmarkNode* GetNodeById(int64_t id) const = 0;

  // Returns the nodes that make up the permanent folder `type` (e.g. its local
  // and account nodes), or an empty vector if the embedder doesn't have that
  // folder. `type` must not be kUnknown.
  virtual std::vector<const bookmarks::BookmarkNode*> GetUnderlyingNodes(
      bookmarks_api::mojom::PermanentFolderType type) const = 0;

  // Asynchronously checks whether bookmarks can be pasted from the clipboard
  // into the folder that a context menu for `selection` would paste into.
  // `selection` must not be empty.
  virtual void CanPasteFromClipboard(
      const std::vector<const bookmarks::BookmarkNode*>& selection,
      base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace bookmark_bar_api

#endif  // COMPONENTS_BROWSER_APIS_UI_CONTROLLERS_BOOKMARK_BAR_ADAPTERS_MODEL_ADAPTER_H_
