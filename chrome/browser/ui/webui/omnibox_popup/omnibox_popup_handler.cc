// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "ui/base/models/menu_model.h"

OmniboxPopupHandler::OmniboxPopupHandler(
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver,
    mojo::PendingRemote<omnibox_popup::mojom::Page> page,
    content::WebContents* web_contents,
    OmniboxController* controller)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_contents_(web_contents),
      controller_(controller) {}

OmniboxPopupHandler::~OmniboxPopupHandler() = default;

void OmniboxPopupHandler::ShowContextMenu(const gfx::Point& point) {
  if (embedder_) {
    embedder_->ShowContextMenu(point, nullptr);
  }
}

void OmniboxPopupHandler::CloseUI() {
  if (embedder_) {
    embedder_->CloseUI();
  }
}

void OmniboxPopupHandler::OnManualBlur(uint32_t sequence_number) {
  // Guard 1: Sequence Guard. Reject stale manual blurs originating from a
  // previous tab state (e.g., during rapid tab switching).
  if (sequence_number < current_sequence_number_) {
    return;
  }

  if (controller_) {
    // TODO(b/527049398): Decouple from the edit model. It might make the most
    // sense to use a delegate interface implemented by the view here.
    if (auto* popup_view = controller_->edit_model()->popup_view()) {
      popup_view->OnManualBlur();
    }
  }
}

void OmniboxPopupHandler::OnSelectionChanged(const gfx::Range& selection,
                                             uint32_t sequence_number,
                                             bool show_full_url) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  latest_selection_ = selection;
  show_full_url_ = show_full_url;
}

void OmniboxPopupHandler::Revert(uint32_t sequence_number) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  if (controller_) {
    // TODO(b/527049398): Decouple from the edit model. It might make the most
    // sense to use a delegate interface implemented by the view here.
    controller_->edit_model()->SetUserText(std::u16string());
  }
}

void OmniboxPopupHandler::OnShow() {
  page_->OnShow();
}

void OmniboxPopupHandler::OnContextMenuClosed() {
  page_->OnContextMenuClosed();
}

void OmniboxPopupHandler::SetInputState(
    const std::string& text,
    const gfx::Range& selection,
    bool user_input_in_progress,
    const std::string& full_url,
    bool is_focused,
    const std::string& permanent_display_text,
    bool show_full_url) {
  latest_selection_ = selection;
  show_full_url_ = show_full_url;
  current_sequence_number_++;
  auto state = omnibox_popup::mojom::OmniboxInputState::New();
  state->sequence_number = current_sequence_number_;
  state->text = text;
  state->selection = selection;
  state->user_input_in_progress = user_input_in_progress;
  state->full_url = full_url;
  state->is_focused = is_focused;
  state->permanent_display_text = permanent_display_text;
  state->show_full_url = show_full_url;
  page_->SetInputState(std::move(state));
}

void OmniboxPopupHandler::LogEscapeAction(
    omnibox_popup::mojom::OmniboxEscapeAction action) {
  base::UmaHistogramEnumeration("Omnibox.Escape", action);
}
