// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
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
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"

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
  // Intentional no-op. Content updates are handled via `SyncNativeStateToWebUI`
  // called directly from specific events (focus, tab switch).
}

void OmniboxPopupViewFullWebUI::SyncNativeStateToWebUI() {
  controller()->edit_model()->ResetDisplayTexts();
  auto* popup_handler = GetPopupHandler();
  if (!popup_handler) {
    return;
  }

  bool user_input_in_progress =
      controller()->edit_model()->user_input_in_progress();
  std::u16string permanent_display_text =
      controller()->edit_model()->GetPermanentDisplayText();
  std::u16string text = user_input_in_progress
                            ? controller()->edit_model()->user_text()
                            : permanent_display_text;
  gfx::Range selection = gfx::Range(0, text.length());
  if (auto* omnibox_view_views =
          static_cast<OmniboxViewViews*>(omnibox_view_)) {
    selection = omnibox_view_views->GetSelectedRange();
  }
  const std::u16string full_url = controller()->client()->GetFormattedFullURL();

  // `last_sent_text_` is null after a state reset (e.g., tab switch).
  // Otherwise, check if `text` or `selection` has diverged.
  bool text_changed = !last_sent_text_ || text != *last_sent_text_;
  bool selection_changed = selection != popup_handler->latest_selection();

  if (text_changed || selection_changed) {
    // TODO(crbug.com/497883783): Consider adding a dedicated
    // `SetSelectionRange` IPC method so that when only the selection
    // changes (e.g. during double clicks or mouse dragging), we do not push
    // the input text and risk resetting DOM input state or scroll position.
    popup_handler->SetInputState(
        base::UTF16ToUTF8(text), selection, user_input_in_progress,
        base::UTF16ToUTF8(full_url), controller()->edit_model()->has_focus(),
        base::UTF16ToUTF8(permanent_display_text), /*show_full_url=*/false);
    last_sent_text_ = text;
  }
}

void OmniboxPopupViewFullWebUI::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

  auto* edit_model = controller()->edit_model();
  bool is_popup_open = controller()->popup_state_manager()->popup_state() ==
                       OmniboxPopupState::kFull;
  bool logically_focused = is_popup_focused_;

  const OmniboxEditModel::State default_state =
      edit_model->GetStateForTabSwitch();
  std::unique_ptr<OmniboxEditModel::State> state;

  // The text input lives in WebUI, so the native view hierarchy does not
  // track selection. Fetch it from the WebUI handler.
  gfx::Range selection;
  bool show_full_url = false;
  if (auto* popup_handler = GetPopupHandler()) {
    selection = popup_handler->latest_selection();
    show_full_url = popup_handler->show_full_url();
  }

  if (is_popup_open && logically_focused) {
    // For an active focused draft, save the draft and mark the focus state
    // as `OMNIBOX_FOCUS_VISIBLE` to restore focus to the omnibox on
    // switch-back. If the draft is empty, restore the permanent URL instead.
    std::u16string user_text = edit_model->user_text();
    if (!user_text.empty()) {
      state = std::make_unique<OmniboxEditModel::State>(
          /*user_input_in_progress=*/true, user_text, default_state.keyword,
          default_state.keyword_placeholder, default_state.keyword_state,
          default_state.keyword_mode_entry_method, OMNIBOX_FOCUS_VISIBLE,
          default_state.autocomplete_input);
    } else {
      // If input is empty, save as focused but not in-progress so it reverts
      // to the permanent URL.
      state = std::make_unique<OmniboxEditModel::State>(
          /*user_input_in_progress=*/false, u"", default_state.keyword,
          default_state.keyword_placeholder, default_state.keyword_state,
          default_state.keyword_mode_entry_method, OMNIBOX_FOCUS_VISIBLE,
          default_state.autocomplete_input);
      if (edit_model->user_input_in_progress()) {
        // Override the selection to be the full length of the permanent URL.
        std::u16string permanent_text = edit_model->GetPermanentDisplayText();
        selection = gfx::Range(0, permanent_text.length());
      }
    }
  } else if (edit_model->user_input_in_progress()) {
    // For an active uncommitted draft where the webpage has focus, preserve
    // the draft and keep the popup open, but restore focus to the webpage.
    // Override the inactive native textfield's state with the active WebUI
    // draft.
    std::u16string override_text = edit_model->user_text();
    state = std::make_unique<OmniboxEditModel::State>(
        /*user_input_in_progress=*/true, override_text, default_state.keyword,
        default_state.keyword_placeholder, default_state.keyword_state,
        default_state.keyword_mode_entry_method, default_state.focus_state,
        default_state.autocomplete_input);
  } else {
    // For a closed and unfocused popup with no draft, save the default native
    // state.
    state = std::make_unique<OmniboxEditModel::State>(default_state);
  }

  // WebUI retains selection across focus changes, so we only need to sync
  // the active selection. `saved_selection_for_focus_change` is unused.
  tab->SetUserData(
      OmniboxTabHelper::kOmniboxStateKey,
      std::make_unique<OmniboxState>(
          *state, selection,
          /*saved_selection_for_focus_change=*/gfx::Range::InvalidRange(),
          show_full_url));
}

void OmniboxPopupViewFullWebUI::OnTabChanged(content::WebContents* contents) {
  last_sent_text_.reset();

  OmniboxPopupState target_popup_state;
  auto* state = static_cast<OmniboxState*>(
      contents->GetUserData(OmniboxTabHelper::kOmniboxStateKey));
  bool should_focus_popup = false;

  if (state) {
    // Restore the saved state for the tab.
    controller()->edit_model()->RestoreState(&state->model_state);

    // Prevent focus state leaks by explicitly syncing the `OmniboxEditModel`'s
    // focus state with the restored state of the newly active tab.
    if (state->model_state.focus_state != OMNIBOX_FOCUS_NONE) {
      controller()->edit_model()->OnSetFocus(/*control_down=*/false);
    } else {
      controller()->edit_model()->OnKillFocus();
    }

    // Only request native keyboard focus for the omnibox
    // popup if it was logically focused when the user switched tabs.
    should_focus_popup = (state->model_state.focus_state != OMNIBOX_FOCUS_NONE);

    // The popup must be visible (`OmniboxPopupState::kFull`) if there is an
    // active draft or if the omnibox should have focus.
    target_popup_state =
        (state->model_state.user_input_in_progress || should_focus_popup)
            ? OmniboxPopupState::kFull
            : OmniboxPopupState::kNone;
  } else {
    // No saved state, so revert to default and re-evaluate popup visibility
    // based on current focus.
    controller()->edit_model()->Revert();
    controller()->edit_model()->OnChanged();
    target_popup_state = controller()->edit_model()->has_focus()
                             ? OmniboxPopupState::kFull
                             : OmniboxPopupState::kNone;
    should_focus_popup = controller()->edit_model()->has_focus();
  }

  is_popup_focused_ = should_focus_popup;

  // TODO(b/504668582): Fix flicker that occurs when switching between two tabs
  //   that have an Omnibox with text.
  controller()->popup_state_manager()->SetPopupState(target_popup_state);

  // Request focus before pushing content state so our `SetInputState` IPC
  // overrides any OS-default focus selection (such as macOS Select-All).
  if (target_popup_state == OmniboxPopupState::kFull && should_focus_popup) {
    presenter()->RequestFocus();
  }

  // Push the restored state to the WebUI handler so it can render the
  // correct text and selection range for the newly selected tab.
  if (auto* popup_handler = GetPopupHandler()) {
    bool user_input_in_progress =
        state ? state->model_state.user_input_in_progress : false;
    std::u16string permanent_display_text =
        controller()->edit_model()->GetPermanentDisplayText();
    std::u16string text = user_input_in_progress ? state->model_state.user_text
                                                 : permanent_display_text;
    bool show_full_url = state ? state->show_full_url : false;
    const std::u16string full_url =
        controller()->client()->GetFormattedFullURL();
    gfx::Range selection = state ? state->selection : gfx::Range(0, 0);
    popup_handler->SetInputState(
        base::UTF16ToUTF8(text), selection, user_input_in_progress,
        base::UTF16ToUTF8(full_url), should_focus_popup,
        base::UTF16ToUTF8(permanent_display_text), show_full_url);
    last_sent_text_ = text;
  }
}

void OmniboxPopupViewFullWebUI::OnFocus() {
  is_popup_focused_ = true;
  bool changed = controller()->popup_state_manager()->popup_state() !=
                 OmniboxPopupState::kFull;

  if (changed) {
    last_sent_text_.reset();
  }

  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kFull);

  if (changed) {
    SyncNativeStateToWebUI();
  }
}

void OmniboxPopupViewFullWebUI::OnManualBlur() {
  // Ignore blurs caused by browser window deactivation to preserve focus state.
  content::WebContents* web_contents =
      (presenter() && presenter()->GetWebUIContent())
          ? presenter()->GetWebUIContent()->GetWebContents()
          : nullptr;
  BrowserWindowInterface* browser_window =
      webui::GetBrowserWindowInterface(web_contents);
  if (browser_window && !browser_window->IsActive()) {
    return;
  }

  // Ignore blurs if there's an active permission prompt.
  if (web_contents) {
    auto* permission_manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents);
    if (permission_manager && permission_manager->IsRequestInProgress()) {
      return;
    }
  }

  is_popup_focused_ = false;

  // Shift keyboard focus to the webpage, but keep the popup open with the
  // draft preserved if `user_input_in_progress()` is true. Otherwise, revert
  // the view and close the popup.
  controller()->edit_model()->OnKillFocus();
  controller()->client()->FocusWebContents();
  if (!controller()->edit_model()->user_input_in_progress()) {
    if (auto* view = controller()->edit_model()->view()) {
      view->RevertAll();
    }
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

OmniboxPopupHandler* OmniboxPopupViewFullWebUI::GetPopupHandler() {
  if (!presenter() || !presenter()->GetWebUIContent()) {
    return nullptr;
  }
  auto* contents_wrapper = presenter()->GetWebUIContent()->contents_wrapper();
  auto* webui_controller =
      contents_wrapper ? contents_wrapper->GetWebUIController() : nullptr;
  auto* popup_ui = static_cast<OmniboxPopupUI*>(webui_controller);
  return popup_ui ? popup_ui->popup_handler() : nullptr;
}
