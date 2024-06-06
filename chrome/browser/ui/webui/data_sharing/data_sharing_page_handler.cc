// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"

DataSharingPageHandler::DataSharingPageHandler(
    TopChromeWebUIController* webui_controller,
    mojo::PendingReceiver<data_sharing::mojom::PageHandler> receiver)
    : webui_controller_(webui_controller),
      receiver_(this, std::move(receiver)) {}

DataSharingPageHandler::~DataSharingPageHandler() = default;

void DataSharingPageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}
