// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/desktop_bookmark_bar_model_adapter.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"
#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_node_id_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"

DesktopBookmarkBarModelAdapter::DesktopBookmarkBarModelAdapter(
    BookmarkMergedSurfaceService* service)
    : service_(service) {}

DesktopBookmarkBarModelAdapter::~DesktopBookmarkBarModelAdapter() = default;

const bookmarks::BookmarkNode* DesktopBookmarkBarModelAdapter::GetNodeById(
    int64_t id) const {
  return bookmarks::GetBookmarkNodeByID(service_->bookmark_model(), id);
}

std::vector<const bookmarks::BookmarkNode*>
DesktopBookmarkBarModelAdapter::GetUnderlyingNodes(
    bookmarks_api::mojom::PermanentFolderType type) const {
  return service_->GetUnderlyingNodes(chrome::ToFolder(
      FromMojoPermanentFolderType(type), service_->bookmark_model()));
}

void DesktopBookmarkBarModelAdapter::CanPasteFromClipboard(
    const std::vector<const bookmarks::BookmarkNode*>& selection,
    base::OnceCallback<void(bool)> callback) {
  // Check the same folder that the context menu's Paste command uses.
  std::unique_ptr<BookmarkParentFolder> parent =
      BookmarkContextMenuController::GetParentForNewNodes(
          base::ToVector(selection, [](const bookmarks::BookmarkNode* node) {
            return raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>(
                node);
          }));
  CHECK(parent);
  BookmarkUIOperationsHelperMergedSurfaces(service_, parent.get())
      .CanPasteFromClipboard(std::move(callback));
}
