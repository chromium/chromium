// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/search_bubble_page_handler.h"

namespace lens {

SearchBubblePageHandler::SearchBubblePageHandler(
    TopChromeWebUIController* webui_controller,
    mojo::PendingReceiver<lens::mojom::SearchBubblePageHandler> receiver,
    mojo::PendingRemote<lens::mojom::SearchBubblePage> page)
    : webui_controller_(webui_controller),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

SearchBubblePageHandler::~SearchBubblePageHandler() = default;

void SearchBubblePageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

}  // namespace lens
