// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_composebox_controller.h"

#include "chrome/browser/ui/lens/lens_composebox_handler.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"

namespace lens {

LensComposeboxController::LensComposeboxController(
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}

LensComposeboxController::~LensComposeboxController() = default;

void LensComposeboxController::BindComposebox(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  // The composebox handler should only be bound once.
  CHECK(composebox_handler_ == nullptr);
  composebox_handler_ = std::make_unique<LensComposeboxHandler>(
      this, std::move(pending_handler), std::move(pending_page),
      std::move(pending_searchbox_handler));
}

}  // namespace lens
