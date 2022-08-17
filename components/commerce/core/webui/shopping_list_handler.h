// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_
#define COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_

#include "components/commerce/core/mojom/shopping_list.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace commerce {

class ShoppingListHandler : public shopping_list::mojom::ShoppingListHandler {
 public:
  explicit ShoppingListHandler(
      mojo::PendingReceiver<shopping_list::mojom::ShoppingListHandler>
          receiver);
  ShoppingListHandler(const ShoppingListHandler&) = delete;
  ShoppingListHandler& operator=(const ShoppingListHandler&) = delete;
  ~ShoppingListHandler() override;

  // shopping_list::mojom::ShoppingListHandler:
  void GetAllBookmarkProductInfo(
      GetAllBookmarkProductInfoCallback callback) override;

 private:
  mojo::Receiver<shopping_list::mojom::ShoppingListHandler> receiver_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEBUI_SHOPPING_LIST_HANDLER_H_
