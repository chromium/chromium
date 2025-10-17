// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"

#include <utility>

ReloadButtonPageHandler::ReloadButtonPageHandler(
    mojo::PendingReceiver<reload_button::mojom::PageHandler> receiver,
    mojo::PendingRemote<reload_button::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

ReloadButtonPageHandler::~ReloadButtonPageHandler() = default;

void ReloadButtonPageHandler::Reload() {
  // TODO(crbug.com/444358999): implement the reload
}

void ReloadButtonPageHandler::StopReload() {
  // TODO(crbug.com/444358999): implement the reload
}
