// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::ComposeboxPageHandler> handler,
    std::unique_ptr<ComposeboxQueryController> query_controller)
    : handler_(this, std::move(handler)),
      query_controller_(std::move(query_controller)) {}

ComposeboxHandler::~ComposeboxHandler() = default;

void ComposeboxHandler::NotifySessionStarted() {
  query_controller_->NotifySessionStarted();
}

void ComposeboxHandler::NotifySessionAbandoned() {
  query_controller_->NotifySessionAbandoned();
}
