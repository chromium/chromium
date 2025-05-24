// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog_delegate.h"

#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

BookmarkAccountStorageMoveDialogDelegate::
    BookmarkAccountStorageMoveDialogDelegate(
        Browser* browser,
        const bookmarks::BookmarkNode* source,
        const bookmarks::BookmarkNode* destination)
    : browser_(browser),
      dialog_source_node_(source),
      dialog_destination_node_(destination) {
  CHECK(browser);
  bookmark_service_ =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(browser->GetProfile());
  if (!bookmark_service_) {
    return;
  }
  observation_.Observe(bookmark_service_);
}

BookmarkAccountStorageMoveDialogDelegate::
    ~BookmarkAccountStorageMoveDialogDelegate() = default;

void BookmarkAccountStorageMoveDialogDelegate::CloseDialog() {
  if (this->dialog_model() && this->dialog_model()->host()) {
    this->dialog_model()->host()->Close();
  }
}

void BookmarkAccountStorageMoveDialogDelegate::BookmarkNodesRemoved(
    const BookmarkParentFolder& parent,
    const base::flat_set<const bookmarks::BookmarkNode*>& nodes) {
  for (const bookmarks::BookmarkNode* node : nodes) {
    // If the dialog nodes OR any of their ancestors them is removed, the move
    // operation is no longer valid. This check is necessary because on removal
    // of a non-permanent bookmark folder, `BookmarkNodesRemoved` is not
    // recursively invoked for its descendants.
    if (dialog_source_node_->HasAncestor(node) ||
        dialog_destination_node_->HasAncestor(node)) {
      // Close the dialog to cancel the move.
      CloseDialog();
      return;
    }
  }
}
