// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_
#define COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/commerce/core/mojom/shopping_list.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace commerce {

class ShoppingService;

class ShoppingListHandler : public shopping_list::mojom::ShoppingListHandler {
 public:
  ShoppingListHandler(
      mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler> receiver,
      bookmarks::BookmarkModel* bookmark_model,
      ShoppingService* shopping_service,
      const std::string& locale);
  ShoppingListHandler(const ShoppingListHandler&) = delete;
  ShoppingListHandler& operator=(const ShoppingListHandler&) = delete;
  ~ShoppingListHandler() override;

  // shopping_list::mojom::ShoppingListHandler:
  void GetAllPriceTrackedBookmarkProductInfo(
      GetAllPriceTrackedBookmarkProductInfoCallback callback) override;
  void TrackPriceForBookmark(int64_t bookmark_id) override;
  void UntrackPriceForBookmark(int64_t bookmark_id) override;

  static std::vector<shopping_list::mojom::BookmarkProductInfoPtr>
  BookmarkListToMojoList(
      bookmarks::BookmarkModel& model,
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
      const std::string& locale);

 private:
  mojo::Receiver<shopping_list::mojom::ShoppingListHandler> receiver_;
  // The bookmark model and shopping service will outlive this implementation
  // since it is a keyed service bound to the browser context (which in turn has
  // the same lifecycle as the browser). The web UI that hosts this will be shut
  // down prior to the rest of the browser.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<ShoppingService> shopping_service_;
  const std::string locale_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_
