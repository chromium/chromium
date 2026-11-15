// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_page_handler.h"

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"
#include "ui/gfx/geometry/rect.h"

OmniboxEverywherePageHandler::OmniboxEverywherePageHandler(
    mojo::PendingReceiver<omnibox_everywhere::mojom::PageHandler> receiver,
    mojo::PendingRemote<omnibox_everywhere::mojom::Page> page,
    OmniboxEverywhereUI* web_ui_controller)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_controller_(web_ui_controller) {}

OmniboxEverywherePageHandler::~OmniboxEverywherePageHandler() = default;

void OmniboxEverywherePageHandler::ShowContextActionMenu(
    const gfx::Rect& anchor_rect) {
  if (web_ui_controller_) {
    web_ui_controller_->ShowContextActionMenu(anchor_rect);
  }
}

void OmniboxEverywherePageHandler::ShowOverflowMenu(
    const gfx::Rect& anchor_rect,
    ui::mojom::MenuSourceType source_type) {
  if (web_ui_controller_) {
    web_ui_controller_->ShowOverflowMenu(anchor_rect, source_type);
  }
}

void OmniboxEverywherePageHandler::SetIsComposebox(bool is_composebox) {
  if (web_ui_controller_) {
    web_ui_controller_->SetIsComposebox(is_composebox);
  }
}

void OmniboxEverywherePageHandler::OnDismissMostVisited() {
  if (web_ui_controller_) {
    web_ui_controller_->OnDismissMostVisited();
  }
}

void OmniboxEverywherePageHandler::OnContextMenuClosed() {
  if (page_) {
    page_->OnContextMenuClosed();
  }
}

void OmniboxEverywherePageHandler::OnOverflowMenuClosed() {
  if (page_) {
    page_->OnOverflowMenuClosed();
  }
}

void OmniboxEverywherePageHandler::OpenComposebox(
    omnibox_everywhere::mojom::ComposeboxInitialStatePtr initial_state) {
  if (page_) {
    page_->OpenComposebox(std::move(initial_state));
  }
}

void OmniboxEverywherePageHandler::OnScreenshotCaptureCancelled() {
  if (page_) {
    page_->OnScreenshotCaptureCancelled();
  }
}

void OmniboxEverywherePageHandler::FocusInput() {
  if (page_) {
    page_->FocusInput();
  }
}

void OmniboxEverywherePageHandler::OnDemoted() {
  if (page_) {
    page_->OnDemoted();
  }
}
