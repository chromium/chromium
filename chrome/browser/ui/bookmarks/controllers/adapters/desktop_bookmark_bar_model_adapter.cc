// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/adapters/desktop_bookmark_bar_model_adapter.h"

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/ui/bookmarks/bookmark_ui_operations_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "components/bookmarks/browser/bookmark_utils.h"

DesktopBookmarkBarModelAdapter::DesktopBookmarkBarModelAdapter(
    BookmarkMergedSurfaceService* service)
    : service_(service) {}

DesktopBookmarkBarModelAdapter::~DesktopBookmarkBarModelAdapter() = default;

bool DesktopBookmarkBarModelAdapter::IsLoaded() const {
  return service_ && service_->loaded();
}

const bookmarks::BookmarkNode* DesktopBookmarkBarModelAdapter::GetNodeById(
    int64_t id) const {
  return service_
             ? bookmarks::GetBookmarkNodeByID(service_->bookmark_model(), id)
             : nullptr;
}

std::vector<const bookmarks::BookmarkNode*>
DesktopBookmarkBarModelAdapter::GetUnderlyingNodes(
    const bookmarks_api::BookmarkParentFolderId& folder_id) const {
  if (!service_) {
    return {};
  }
  BookmarkParentFolder folder =
      chrome::ToFolder(folder_id, service_->bookmark_model());
  return service_->GetUnderlyingNodes(folder);
}

void DesktopBookmarkBarModelAdapter::CanPasteFromClipboard(
    const BookmarkParentFolder* parent,
    base::OnceCallback<void(bool)> callback) {
  if (!service_) {
    std::move(callback).Run(false);
    return;
  }
  BookmarkUIOperationsHelperMergedSurfaces(service_, parent)
      .CanPasteFromClipboard(std::move(callback));
}
