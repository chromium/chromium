// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_model_adapter.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"

namespace {

using bookmarks::BookmarkNode;
using bookmarks_api::mojom::PermanentFolderType;

// Returns `account_node` (if any) followed by `local_node`.
std::vector<const BookmarkNode*> AccountThenLocal(
    const BookmarkNode* account_node,
    const BookmarkNode* local_node) {
  if (!account_node) {
    return {local_node};
  }
  return {account_node, local_node};
}

}  // namespace

AndroidBookmarkBarModelAdapter::AndroidBookmarkBarModelAdapter(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed_service)
    : model_(model), managed_service_(managed_service) {
  CHECK(model_);
  CHECK(managed_service_);
}

AndroidBookmarkBarModelAdapter::~AndroidBookmarkBarModelAdapter() = default;

const BookmarkNode* AndroidBookmarkBarModelAdapter::GetNodeById(
    int64_t id) const {
  return bookmarks::GetBookmarkNodeByID(model_, id);
}

std::vector<const BookmarkNode*>
AndroidBookmarkBarModelAdapter::GetUnderlyingNodes(
    PermanentFolderType type) const {
  switch (type) {
    case PermanentFolderType::kBookmarkBar:
      return AccountThenLocal(model_->account_bookmark_bar_node(),
                              model_->bookmark_bar_node());
    case PermanentFolderType::kOther:
      return AccountThenLocal(model_->account_other_node(),
                              model_->other_node());
    case PermanentFolderType::kMobile:
      return AccountThenLocal(model_->account_mobile_node(),
                              model_->mobile_node());
    case PermanentFolderType::kManaged:
      if (managed_service_->managed_node()) {
        return {managed_service_->managed_node()};
      }
      return {};
    case PermanentFolderType::kUnknown:
      break;
  }
  NOTREACHED();
}
