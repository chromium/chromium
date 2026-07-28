// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_full_webui.h"

#include <string>

#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_full_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_class_properties.h"

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
  bool focus = controller()->edit_model()->has_focus();
  // Default to select-all if focused so that taking focus selects all text by
  // default (whether permanent URL or draft). Otherwise default to empty
  // selection.
  gfx::Range selection =
      focus ? gfx::Range(0, text.length()) : gfx::Range(0, 0);
  // If the user is actively typing a draft or has an active highlight from
  // mouse dragging or double-clicking, use the native view's selection range.
  if (auto* omnibox_view_views =
          static_cast<OmniboxViewViews*>(omnibox_view_)) {
    if (user_input_in_progress || omnibox_view_views->HasSelection()) {
      selection = omnibox_view_views->GetSelectedRange();
    }
  }
  const std::u16string full_url = controller()->client()->GetFormattedFullURL();

  // `last_sent_text_` is null after a state reset (e.g., tab switch).
  // Otherwise, check if `text`, `selection`, or `focus` has diverged.
  bool text_changed = !last_sent_text_ || text != *last_sent_text_;
  bool selection_changed = selection != popup_handler->latest_selection();
  bool focus_changed = !last_sent_focus_ || focus != *last_sent_focus_;

  if (text_changed || selection_changed || focus_changed) {
    // TODO(crbug.com/497883783): Consider adding a dedicated
    // `SetSelectionRange` IPC method so that when only the selection
    // changes (e.g. during double clicks or mouse dragging), we do not push
    // the input text and risk resetting DOM input state or scroll position.
    popup_handler->SetInputState(
        base::UTF16ToUTF8(text), selection, user_input_in_progress,
        base::UTF16ToUTF8(full_url), controller()->edit_model()->has_focus(),
        base::UTF16ToUTF8(permanent_display_text), /*show_full_url=*/false);
    last_sent_text_ = text;
    last_sent_focus_ = focus;
  }
}

void OmniboxPopupViewFullWebUI::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

  auto* edit_model = controller()->edit_model();
  bool logically_focused = edit_model->has_focus();
  bool is_popup_open = controller()->popup_state_manager()->popup_state() ==
                       OmniboxPopupState::kFull;

  // Clicking a tab moves focus to the tab strip before `SaveStateToTab` runs,
  // causing `window blur` and wiping `edit_model->has_focus()`. Unless the
  // webpage (`ContentsWebView`) is focused, the user didn't click the page to
  // blur the omnibox, so preserve logical focus across tab switches.
  if (is_popup_open && !logically_focused) {
    bool webpage_focused = false;
    if (auto* omnibox_view_views =
            static_cast<OmniboxViewViews*>(omnibox_view_)) {
      if (auto* focus_manager = omnibox_view_views->GetFocusManager()) {
        if (views::View* focused_view = focus_manager->GetFocusedView()) {
          for (views::View* v = focused_view; v; v = v->parent()) {
            if (v->GetProperty(views::kElementIdentifierKey) ==
                ContentsWebView::kContentsWebViewElementId) {
              webpage_focused = true;
              break;
            }
          }
        }
      }
    }
    if (!webpage_focused) {
      logically_focused = true;
    }
  }

  // The text input lives in WebUI, so the native view hierarchy does not
  // track selection. Fetch it from the WebUI handler.
  gfx::Range selection;
  bool show_full_url = false;
  if (auto* popup_handler = GetPopupHandler()) {
    selection = popup_handler->latest_selection();
    show_full_url = popup_handler->show_full_url();
  }

  // If the user cleared the Omnibox, we should not preserve the empty draft
  // across tab switches. Explicitly revert the edit model so that switching
  // back to this tab restores the page's permanent URL (or empty for NTP).
  const bool was_cleared_by_user =
      edit_model->user_input_in_progress() && edit_model->user_text().empty();

  const OmniboxEditModel::State default_state =
      edit_model->GetStateForTabSwitch();
  std::unique_ptr<OmniboxEditModel::State> state;

  const OmniboxFocusState target_focus_state =
      logically_focused ? OMNIBOX_FOCUS_VISIBLE : OMNIBOX_FOCUS_NONE;

  state = std::make_unique<OmniboxEditModel::State>(default_state);
  state->focus_state = target_focus_state;

  if (was_cleared_by_user) {
    std::u16string permanent_text = edit_model->GetPermanentDisplayText();
    // If the WebUI input field was physically empty before switching tabs,
    // we must select-all on restoration (since the permanent URL will be
    // restored).
    selection = gfx::Range(0, permanent_text.length());
    // Force `user_input_in_progress` to false to trigger a revert
    // to the permanent URL on restoration.
    state->user_input_in_progress = false;
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
  last_sent_focus_.reset();

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
    // active draft or if the omnibox should have visible focus.
    const bool has_non_empty_draft =
        state->model_state.user_input_in_progress &&
        !state->model_state.user_text.empty();
    target_popup_state = (has_non_empty_draft || should_focus_popup)
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


  // TODO(b/504668582): Fix flicker that occurs when switching between two tabs
  //   that have an Omnibox with text.
  controller()->popup_state_manager()->SetPopupState(target_popup_state);

  // Request focus before pushing content state so our `SetInputState` IPC
  // overrides any OS-default focus selection (such as macOS Select-All).
  if (target_popup_state == OmniboxPopupState::kFull) {
    if (presenter()) {
      presenter()->Show();
      if (should_focus_popup) {
        presenter()->RequestFocus();
      }
    }
  } else {
    if (presenter()) {
      presenter()->Hide();
    }
  }

  // Push the restored state to the WebUI handler so it can render the
  // correct text and selection range for the newly selected tab.
  if (auto* popup_handler = GetPopupHandler()) {
    bool user_input_in_progress =
        state ? state->model_state.user_input_in_progress : false;
    std::u16string user_text = state ? state->model_state.user_text : u"";
    std::u16string permanent_display_text =
        controller()->edit_model()->GetPermanentDisplayText();
    std::u16string text = user_input_in_progress && !user_text.empty()
                              ? user_text
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
    last_sent_focus_ = should_focus_popup;
  }
}

void OmniboxPopupViewFullWebUI::OnFocus() {
  bool changed = controller()->popup_state_manager()->popup_state() !=
                 OmniboxPopupState::kFull;

  if (changed) {
    last_sent_text_.reset();
    last_sent_focus_.reset();
  }

  controller()->edit_model()->OnSetFocus(/*control_down=*/false);
  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kFull);

  if (presenter()) {
    presenter()->Show();
    // Explicitly request focus on the presenter and its WebContents to ensure
    // keyboard events route to the WebUI input field.
    presenter()->RequestFocus();
  }

  if (changed) {
    SyncNativeStateToWebUI();
  } else if (auto* popup_handler = GetPopupHandler()) {
    // If the popup was already open (`!changed`), explicitly send
    // `SetFocus(true)` via IPC to ensure WebUI DOM input element focus is
    // restored if it was lost.
    popup_handler->SetFocus(true);
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
