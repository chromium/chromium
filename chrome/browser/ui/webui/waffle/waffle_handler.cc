// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/waffle/waffle_handler.h"

#include "chrome/browser/signin/signin_features.h"

WaffleHandler::WaffleHandler(
    mojo::PendingReceiver<waffle::mojom::PageHandler> receiver,
    base::OnceClosure display_dialog_callback)
    : receiver_(this, std::move(receiver)),
      display_dialog_callback_(std::move(display_dialog_callback)) {
  CHECK(base::FeatureList::IsEnabled(kWaffle));
  // `display_dialog_callback` being null would indicate that the handler is
  // created before calling `WaffleUI::Initialize()`, which should never happen.
  CHECK(display_dialog_callback_);
}

WaffleHandler::~WaffleHandler() = default;

void WaffleHandler::DisplayDialog() {
  if (display_dialog_callback_) {
    std::move(display_dialog_callback_).Run();
  }
}
