// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion_page_handler.h"

#include "chrome/browser/ui/webui/side_panel/search_companion/search_companion_side_panel_ui.h"

SearchCompanionPageHandler::SearchCompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::SearchCompanionPageHandler>
        receiver,
    SearchCompanionSidePanelUI* search_companion_ui)
    : receiver_(this, std::move(receiver)),
      search_companion_ui_(search_companion_ui) {}

SearchCompanionPageHandler::~SearchCompanionPageHandler() = default;

void SearchCompanionPageHandler::ShowUI() {
  if (auto embedder = search_companion_ui_->embedder()) {
    embedder->ShowUI();
  }
}
