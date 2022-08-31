// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_list_handler.h"
#include "components/commerce/core/price_tracking_utils.h"

#include <vector>
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/shopping_service.h"

namespace commerce {

ShoppingListHandler::ShoppingListHandler(
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver,
    bookmarks::BookmarkModel* bookmark_model,
    ShoppingService* shopping_service)
    : receiver_(this, std::move(receiver)),
      bookmark_model_(bookmark_model),
      shopping_service_(shopping_service) {}

ShoppingListHandler::~ShoppingListHandler() = default;

void ShoppingListHandler::GetAllBookmarkProductInfo(
    GetAllBookmarkProductInfoCallback callback) {
  std::vector<shopping_list::mojom::BookmarkProductInfoPtr> info_list;

  // TODO(crbug.com/1346620): Call actual implementation from ShoppingService to
  // get data.
  std::move(callback).Run(std::move(info_list));
}

void ShoppingListHandler::TrackPriceForBookmark(int64_t bookmark_id) {
  commerce::SetPriceTrackingStateForBookmark(
      shopping_service_, bookmark_model_,
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id), true,
      base::DoNothing());
}

void ShoppingListHandler::UntrackPriceForBookmark(int64_t bookmark_id) {
  commerce::SetPriceTrackingStateForBookmark(
      shopping_service_, bookmark_model_,
      bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id), false,
      base::DoNothing());
}
}  // namespace commerce
