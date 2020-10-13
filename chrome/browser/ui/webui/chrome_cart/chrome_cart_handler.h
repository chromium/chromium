// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_CART_CHROME_CART_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_CART_CHROME_CART_HANDLER_H_

#include "chrome/browser/ui/webui/chrome_cart/chrome_cart.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace bookmarks {
class BookmarkModel;
}

class Profile;

// Handles requests of chrome cart module sent from JS.
class ChromeCartHandler : public chrome_cart::mojom::ChromeCartHandler {
 public:
  ChromeCartHandler(
      mojo::PendingReceiver<chrome_cart::mojom::ChromeCartHandler> handler,
      Profile* profile);
  ~ChromeCartHandler() override;

  // chrome_cart::mojom::ChromeCartHandler:
  void GetData(GetDataCallback callback) override;

  void ShouldShowModule(ShouldShowModuleCallback callback) override;

 private:
  mojo::Receiver<chrome_cart::mojom::ChromeCartHandler> handler_;
  bookmarks::BookmarkModel* bookmark_model_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_CART_CHROME_CART_HANDLER_H_
