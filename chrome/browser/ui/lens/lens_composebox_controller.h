// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class LensSearchController;

namespace lens {

class LensComposeboxHandler;

// Controller for the Lens compose box. This class is responsible for handling
// communications between the Lens WebUI compose box and other Lens components,
// as well as storing any state needed for the compose box. Note: This class is
// different from the LensSearchboxController, which is responsible for the old,
// non-AIM search box.
class LensComposeboxController {
 public:
  explicit LensComposeboxController(
      LensSearchController* lens_search_controller);
  ~LensComposeboxController();

  // This method is used to set up communication between this instance and the
  // compose box WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and has bound the handler.
  void BindComposebox(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler);

 private:
  // Owns this.
  const raw_ptr<LensSearchController> lens_search_controller_;

  // The class responsible for handling messages between the compose box and
  // the WebUI.
  std::unique_ptr<LensComposeboxHandler> composebox_handler_;
};
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_
