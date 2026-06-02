// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "content/public/browser/web_contents.h"

OmniboxPopupViewFullWebUI::OmniboxPopupViewFullWebUI(
    OmniboxView* omnibox_view,
    OmniboxController* controller,
    LocationBar* location_bar,
    OmniboxPopupPresenterDelegate& presenter_delegate)
    : OmniboxPopupViewWebUI(
          omnibox_view,
          controller,
          location_bar,
          presenter_delegate,
          std::make_unique<OmniboxPopupFullPresenter>(location_bar,
                                                      presenter_delegate,
                                                      controller)) {}

OmniboxPopupViewFullWebUI::~OmniboxPopupViewFullWebUI() = default;

void OmniboxPopupViewFullWebUI::UpdatePopupAppearance() {
  // Intentional no-op. Content updates are handled via PushTextToWebUI called
  // directly from specific events (focus, tab switch).
}

void OmniboxPopupViewFullWebUI::PushTextToWebUI() {
  if (is_switching_tab_) {
    return;
  }
  controller()->edit_model()->ResetDisplayTexts();
  auto* webui_controller =
      presenter()->GetWebUIContent()->contents_wrapper()->GetWebUIController();
  auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller);
  if (auto* popup_handler = popup_ui ? popup_ui->popup_handler() : nullptr) {
    std::u16string text =
        controller()->edit_model()->user_input_in_progress()
            ? controller()->edit_model()->user_text()
            : controller()->edit_model()->GetPermanentDisplayText();
    // `last_sent_text_` is null after a state reset (e.g., tab switch).
    // Otherwise, check if `text` has diverged (e.g., via navigation or
    // internal WebUI-triggered model updates).
    bool text_changed = !last_sent_text_ || text != *last_sent_text_;
    if (text_changed) {
      popup_handler->SetInputText(base::UTF16ToUTF8(text));
      last_sent_text_ = text;
    }
  }
}

void OmniboxPopupViewFullWebUI::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);
  is_switching_tab_ = true;
  const OmniboxEditModel::State state =
      controller()->edit_model()->GetStateForTabSwitch();

  // TODO(b/516847801): Fetch the selection range from the WebUI rather than
  //   the native view, as the WebUI is the source of truth for user interaction
  //   in V2.
  tab->SetUserData(
      OmniboxTabHelper::kOmniboxStateKey,
      std::make_unique<OmniboxState>(
          state, /*selection=*/gfx::Range(),
          /*saved_selection_for_focus_change=*/gfx::Range::InvalidRange()));
}

void OmniboxPopupViewFullWebUI::OnTabChanged(content::WebContents* contents) {
  is_switching_tab_ = false;
  last_sent_text_.reset();

  OmniboxPopupState target_popup_state;
  auto* state = static_cast<OmniboxState*>(
      contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  if (state) {
    // Restore the saved state for the tab.
    controller()->edit_model()->RestoreState(&state->model_state);
    target_popup_state = state->model_state.user_input_in_progress
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
    // TODO(b/516847801): Restore text selection in the native view if needed,
    // or ensure WebUI handles it.
    // TODO(b/516847801): Handle special case for empty user text (SelectAll).
  } else {
    // No saved state: revert to default and re-evaluate popup visibility based
    // on current focus.
    controller()->edit_model()->Revert();
    controller()->edit_model()->OnChanged();
    target_popup_state = controller()->edit_model()->has_focus()
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
  }

  // TODO(b/504668582): Fix flicker that occurs when switching between two tabs
  //   that have an Omnibox with text.
  UpdatePopupStateAndContent(target_popup_state);
}

void OmniboxPopupViewFullWebUI::OnFocus() {
  UpdatePopupStateAndContent(OmniboxPopupState::kFull);
}

void OmniboxPopupViewFullWebUI::UpdatePopupStateAndContent(
    OmniboxPopupState state) {
  if (state == OmniboxPopupState::kFull) {
    PushTextToWebUI();
  }
  controller()->popup_state_manager()->SetPopupState(state);
}
