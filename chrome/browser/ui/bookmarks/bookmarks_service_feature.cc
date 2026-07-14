// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmarks_service_feature.h"

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/ui/bookmarks/bookmark_merged_surface_view.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_apis/bookmarks/bookmarks_service_impl.h"

BookmarksServiceFeature::BookmarksServiceFeature(
    BookmarkMergedSurfaceService* merged_service)
    : merged_service_(merged_service) {
  CHECK(merged_service_);
  observation_.Observe(merged_service_);
  if (merged_service_->loaded()) {
    // The model might not be loaded at instantiation, in which case we need
    // to defer service instantiation.
    InitializeService();
  }
}

BookmarksServiceFeature::~BookmarksServiceFeature() = default;

void BookmarksServiceFeature::Accept(
    mojo::PendingReceiver<bookmarks_api::mojom::BookmarksService> receiver) {
  if (bookmarks_service_) {
    bookmarks_service_->Accept(std::move(receiver));
  } else if (merged_service_) {
    queued_receivers_.push_back(std::move(receiver));
  }
}

void BookmarksServiceFeature::BookmarkMergedSurfaceServiceLoaded() {
  InitializeService();
}

void BookmarksServiceFeature::BookmarkMergedSurfaceServiceBeingDeleted() {
  ShutdownService();
}

void BookmarksServiceFeature::BookmarkNodeAdded(
    const BookmarkParentFolder& parent,
    size_t index) {}

void BookmarksServiceFeature::BookmarkNodesRemoved(
    const BookmarkParentFolder& parent,
    const base::flat_set<const bookmarks::BookmarkNode*>& nodes) {}

void BookmarksServiceFeature::BookmarkNodeMoved(
    const BookmarkParentFolder& old_parent,
    size_t old_index,
    const BookmarkParentFolder& new_parent,
    size_t new_index) {}

void BookmarksServiceFeature::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {}

void BookmarksServiceFeature::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {}

void BookmarksServiceFeature::BookmarkParentFolderChildrenReordered(
    const BookmarkParentFolder& folder) {}

void BookmarksServiceFeature::BookmarkAllUserNodesRemoved() {}

void BookmarksServiceFeature::InitializeService() {
  // Safe for multiple calls.
  if (bookmarks_service_) {
    return;
  }
  bookmarks_service_ = std::make_unique<bookmarks_api::BookmarksServiceImpl>(
      std::make_unique<BookmarkMergedSurfaceView>(merged_service_));
  for (auto& receiver : queued_receivers_) {
    bookmarks_service_->Accept(std::move(receiver));
  }
  queued_receivers_.clear();
}

void BookmarksServiceFeature::ShutdownService() {
  observation_.Reset();
  merged_service_ = nullptr;
  bookmarks_service_.reset();
  queued_receivers_.clear();
}
