// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_node_id_conversions.h"

#include <variant>

#include "base/notreached.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"

namespace {

using bookmarks_api::mojom::PermanentFolderType;

PermanentFolderType ToMojo(bookmarks::PermanentFolderType type) {
  switch (type) {
    case bookmarks::PermanentFolderType::kBookmarkBarNode:
      return PermanentFolderType::kBookmarkBar;
    case bookmarks::PermanentFolderType::kOtherNode:
      return PermanentFolderType::kOther;
    case bookmarks::PermanentFolderType::kMobileNode:
      return PermanentFolderType::kMobile;
    case bookmarks::PermanentFolderType::kManagedNode:
      return PermanentFolderType::kManaged;
  }
  NOTREACHED();
}

}  // namespace

bookmarks::PermanentFolderType FromMojoPermanentFolderType(
    bookmarks_api::mojom::PermanentFolderType type) {
  switch (type) {
    case PermanentFolderType::kBookmarkBar:
      return bookmarks::PermanentFolderType::kBookmarkBarNode;
    case PermanentFolderType::kOther:
      return bookmarks::PermanentFolderType::kOtherNode;
    case PermanentFolderType::kMobile:
      return bookmarks::PermanentFolderType::kMobileNode;
    case PermanentFolderType::kManaged:
      return bookmarks::PermanentFolderType::kManagedNode;
    case PermanentFolderType::kUnknown:
      break;
  }
  NOTREACHED();
}

bookmark_bar_api::mojom::NodeIdPtr ToMojoNodeId(
    const bookmarks::BookmarkNodeId& id) {
  if (const int64_t* node_id = std::get_if<int64_t>(&id)) {
    return bookmark_bar_api::mojom::NodeId::NewNodeId(*node_id);
  }
  return bookmark_bar_api::mojom::NodeId::NewPermanentFolder(
      ToMojo(std::get<bookmarks::PermanentFolderType>(id)));
}
