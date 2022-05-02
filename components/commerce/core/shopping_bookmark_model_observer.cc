// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_bookmark_model_observer.h"

#include "components/bookmarks/browser/bookmark_node.h"

namespace {
// TODO(1314355): This constant should be removed in favor of power bookmark
//                support in components.
const char kPowerBookmarkMetaKey[] = "power_bookmark_meta";
}  // namespace

namespace commerce {

ShoppingBookmarkModelObserver::ShoppingBookmarkModelObserver(
    bookmarks::BookmarkModel* model) {
  scoped_observation_.Observe(model);
}

ShoppingBookmarkModelObserver::~ShoppingBookmarkModelObserver() = default;

void ShoppingBookmarkModelObserver::BookmarkModelChanged() {}

void ShoppingBookmarkModelObserver::OnWillChangeBookmarkNode(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  // Since the node is about to change, map its current known URL.
  node_to_url_map_[node->id()] = node->url();
}

void ShoppingBookmarkModelObserver::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (node_to_url_map_[node->id()] != node->url()) {
    // If the URL did change, clear the power bookmark shopping meta.
    // TODO(1314355): This blindly deletes the power bookmark meta without
    //                checking the type. This works while shopping is the
    //                only power bookmark type but will need to be updated.
    model->DeleteNodeMetaInfo(node, kPowerBookmarkMetaKey);
  }

  node_to_url_map_.erase(node->id());
}

}  // namespace commerce
