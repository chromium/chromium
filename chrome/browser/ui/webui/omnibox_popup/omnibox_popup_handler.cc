// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
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
  // Transfer focus from the location bar to the active web tab DOM and notify
  // the edit model that focus was killed so internal focus state and metrics
  // trackers are updated.
  if (controller_) {
    if (controller_->client()) {
      controller_->client()->FocusWebContents();
    }
    if (controller_->edit_model()) {
      controller_->edit_model()->OnKillFocus();
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
    controller_->edit_model()->Revert();
  }
  latest_selection_ = gfx::Range(0, 0);
}

void OmniboxPopupHandler::OnInputCleared(uint32_t sequence_number) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  latest_selection_ = gfx::Range(0, 0);
  show_full_url_ = false;
  if (controller_) {
    controller_->edit_model()->SetUserText(std::u16string());
    // TODO(b/504668292): Vet if this setting of `SetWindowTextAndCaretPos` can
    // be removed. Right now `FullWebUIOmniboxInteractiveTest.ClearAndSwitchTab`
    // relies on this, since it checks if the Views Omnibox has the same string
    // as the WebUI Omnibox, but it might not be needed in production.
    if (controller_->edit_model()->view()) {
      controller_->edit_model()->view()->SetWindowTextAndCaretPos(
          /*text=*/std::u16string(), /*caret_pos=*/0, /*update_popup=*/false,
          /*notify_text_changed=*/false);
    }
  }
}

void OmniboxPopupHandler::OnPaste(const std::string& text,
                                  const gfx::Range& selection,
                                  uint32_t sequence_number) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  gfx::Range prev_selection = latest_selection_;
  latest_selection_ = selection;
  show_full_url_ = false;

  if (controller_ && controller_->edit_model()) {
    OmniboxEditModel* model = controller_->edit_model();
    model->OnPaste();

    // The old text is deliberately set to the "empty string" in order to align
    // with the logic in `OmniboxViewViews::OnOmniboxPasteComplete()`.
    std::u16string u16_old_text;
    std::u16string u16_new_text = base::UTF8ToUTF16(text);

    OmniboxView::StateChanges state_changes;
    state_changes.old_text = &u16_old_text;
    state_changes.new_text = &u16_new_text;
    state_changes.new_selection = selection;
    state_changes.selection_differs =
        (!prev_selection.is_empty() || !selection.is_empty()) &&
        !prev_selection.EqualsIgnoringDirection(selection);
    state_changes.text_differs = u16_old_text != u16_new_text;
    // By definition, a PASTE operation cannot enter/exit keyword mode, so
    // `keyword_differs` is always set to `false`.
    state_changes.keyword_differs = false;
    // Since "old text" is an empty string and "new text" is a (potentially)
    // non-empty string, a PASTE operation is never going to be a deletion, so
    // `just_deleted_text` is always set to `false`.
    state_changes.just_deleted_text = false;

    bool something_changed = model->OnAfterPossibleChange(
        state_changes, /*allow_keyword_ui_change=*/true);

    if (something_changed &&
        (state_changes.text_differs || state_changes.keyword_differs)) {
      // TODO(b/514811525): Trigger URL component emphasis in WebUI.
      model->OnChanged();
    }
  }
}

void OmniboxPopupHandler::RequestInputState() {
  auto* edit_model = controller_ ? controller_->edit_model() : nullptr;
  auto* popup_view = edit_model ? edit_model->popup_view() : nullptr;
  if (popup_view) {
    popup_view->SyncNativeStateToWebUI(/*query_zps=*/false);
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
    bool show_full_url,
    bool query_zps) {
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
  state->query_zps = query_zps;
  page_->SetInputState(std::move(state));
}

void OmniboxPopupHandler::SetFocus(bool is_focused) {
  page_->SetFocus(is_focused);
}

void OmniboxPopupHandler::LogEscapeAction(
    omnibox_popup::mojom::OmniboxEscapeAction action) {
  base::UmaHistogramEnumeration("Omnibox.Escape", action);
}

void OmniboxPopupHandler::OpenAimPopup(bool via_keyboard) {
  if (controller_) {
    controller_->edit_model()->OpenSelection(
        OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                              OmniboxPopupSelection::FOCUSED_BUTTON_AIM),
        via_keyboard);
  }
}
