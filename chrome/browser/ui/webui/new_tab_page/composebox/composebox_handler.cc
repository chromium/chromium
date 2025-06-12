// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

// TODO(420700441) Add unittest coverage.
ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::ComposeboxPageHandler> handler)
    : handler_(this, std::move(handler)),
      query_controller_{std::make_unique<ComposeboxQueryController>()} {}

ComposeboxHandler::~ComposeboxHandler() = default;

void ComposeboxHandler::NotifySessionStarted() {
  query_controller_->NotifySessionStarted();
}
