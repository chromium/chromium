// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_model_adapter.h"

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_node_id_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"

BookmarkBarModelAdapter::BookmarkBarModelAdapter(
    BookmarkMergedSurfaceService* service)
    : service_(service) {}

BookmarkBarModelAdapter::~BookmarkBarModelAdapter() = default;

const bookmarks::BookmarkNode* BookmarkBarModelAdapter::GetNodeById(
    int64_t id) const {
  return bookmarks::GetBookmarkNodeByID(service_->bookmark_model(), id);
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkBarModelAdapter::GetUnderlyingNodes(
    bookmarks_api::mojom::PermanentFolderType type) const {
  return service_->GetUnderlyingNodes(bookmarks::ToFolder(
      FromMojoPermanentFolderType(type), service_->bookmark_model()));
}
