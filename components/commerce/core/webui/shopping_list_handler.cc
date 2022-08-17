// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_list_handler.h"

#include <vector>

namespace commerce {

ShoppingListHandler::ShoppingListHandler(
    mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

ShoppingListHandler::~ShoppingListHandler() = default;

void ShoppingListHandler::GetAllBookmarkProductInfo(
    GetAllBookmarkProductInfoCallback callback) {
  std::vector<shopping_list::mojom::BookmarkProductInfoPtr> info_list;

  // TODO(crbug.com/1346620): Call actual implementation from ShoppingService to
  // get data.
  std::move(callback).Run(std::move(info_list));
}
}  // namespace commerce
