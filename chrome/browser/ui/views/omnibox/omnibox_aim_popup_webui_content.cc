// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

OmniboxAimPopupWebUIContent::OmniboxAimPopupWebUIContent(
    OmniboxPopupPresenterBase* presenter,
    LocationBar* location_bar,
    OmniboxController* controller)
    : OmniboxPopupWebUIBaseContent(presenter,
                                   location_bar,
                                   controller,
                                   /*top_rounded_corners=*/true) {
  SetContentURL(chrome::kChromeUIOmniboxPopupAimURL);
}

OmniboxAimPopupWebUIContent::~OmniboxAimPopupWebUIContent() = default;

void OmniboxAimPopupWebUIContent::Clear() {
  auto* handler = popup_aim_handler();
  if (handler) {
    // Pass the original web contents captured during ShowUI to handle
    // underlying changes to referenced web contents due to async events.
    handler->ClearPopup(
        base::BindOnce(&OmniboxAimPopupWebUIContent::OnClearCallback,
                       weak_factory_.GetWeakPtr(), active_web_contents_));
  } else {
    Detach();
  }
}

void OmniboxAimPopupWebUIContent::OnContextMenuClosed() {
  if (auto* handler = popup_aim_handler()) {
    handler->OnContextMenuClosed();
  }
}

void OmniboxAimPopupWebUIContent::OnClearCallback(
    base::WeakPtr<content::WebContents> original_web_contents,
    const std::string& input) {
  // Now that the WebUI has painted, it is safe to detach and cleanup.
  Detach();

  const bool draft_already_applied = draft_applied_on_close_;
  draft_applied_on_close_ = false;

  // Check if tabs have switched due to an async event.
  // Navigation to another tab is an async process which leads to a race
  // condition with the cleanup of the omnibox aim webui popup and which
  // web contents is referenced by the omnibox_edit_model.
  if (location_bar()->GetWebContents() == original_web_contents.get()) {
    if (!draft_already_applied) {
      ApplyInputAndCleanup(input);
    }
  } else if (original_web_contents && !input.empty()) {
    SaveInputToBackgroundTab(original_web_contents.get(), input);
  }
}

void OmniboxAimPopupWebUIContent::SaveInputToBackgroundTab(
    content::WebContents* original_web_contents,
    const std::string& input) {
  OmniboxViewViews::SetUserTextForTab(original_web_contents,
                                      base::UTF8ToUTF16(input));
}

void OmniboxAimPopupWebUIContent::ApplyInputAndCleanup(
    const std::string& input) {
  const bool is_full_webui = is_full_webui_omnibox();
  if (is_full_webui) {
    controller()->edit_model()->Revert();
  } else {
    location_bar()->GetOmniboxView()->RevertAll();
  }
  if (!input.empty()) {
    location_bar()->GetOmniboxView()->SetUserText(base::UTF8ToUTF16(input),
                                                  /*update_popup=*/false);
  }

  if (is_full_webui) {
    // Hand focus back to the omnibox. Don't select all so the caret continues
    // the user's editing session.
    if (auto* popup_view = location_bar()->GetOmniboxPopupView()) {
      popup_view->OnFocus(/*query_zps=*/false, /*select_all=*/false);
    }
  }
}

void OmniboxAimPopupWebUIContent::FocusInput() {
  if (auto* handler = popup_aim_handler()) {
    handler->FocusInput();
  }
}

std::string_view OmniboxAimPopupWebUIContent::GetMetricPrefix() const {
  return "Omnibox.Popup.Aim";
}

void OmniboxAimPopupWebUIContent::UpdateLocationBarFocusForScreenReader() {
  if (is_full_webui_omnibox()) {
    // `ApplyInputAndCleanup()` unconditionally hands focus back to the omnibox
    // after applying the draft text. Focusing early here would transition the
    // popup state to `kFull` before the draft is set on the `OmniboxEditModel`.
    return;
  }
  if (GetWidget() &&
      GetWidget()->ShouldHandleNativeWidgetActivationChanged(false) &&
      GetWidget()->IsActive()) {
    const bool is_screen_reader_enabled =
        content::BrowserAccessibilityState::GetInstance()
            ->GetAccessibilityMode()
            .has_mode(ui::AXMode::kScreenReader);
    if (is_screen_reader_enabled) {
      location_bar()->FocusLocation(/*is_user_initiated=*/true,
                                    /*clear_focus_if_failed=*/false);
    }
  }
}

bool OmniboxAimPopupWebUIContent::EscClosesUI() const {
  // The WebUI handles ESC so the close routes through `RequestClose()` and
  // carries the draft text with it.
  return !is_full_webui_omnibox();
}

void OmniboxAimPopupWebUIContent::CloseUI() {
  // If the popup state is not shown, don't take any action. Closing the UI
  // multiple times can result in incorrect state transitions from OnClose.
  if (!IsShown()) {
    return;
  }

  set_is_shown(false);

  if (is_full_webui_omnibox()) {
    std::string draft;
    if (auto* handler = popup_aim_handler()) {
      draft = handler->cached_draft_text();
      handler->clear_cached_draft_text();
    }

    // Must be set before `ApplyInputAndCleanup()`, which can synchronously
    // re-enter `Clear()`.
    draft_applied_on_close_ = true;
    ApplyInputAndCleanup(draft);
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kFull);
  } else {
    // The legacy popup applies the draft from the `ClearPopup()` reply in
    // `OnClearCallback()`.
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
}

// Override of WebUIContentsWrapper::Host::HandleContextMenu. This mirrors
// content::WebContentsDelegate::HandleContextMenu, which is called by the
// WebContentsImpl to allow the delegate to handle the context menu if desired.
// Returning true means the context menu request was handled (and thus
// the caller suppresses their own context menu). Returning false allows
// the default context menu to be shown.
bool OmniboxAimPopupWebUIContent::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppress the context menu unless it's on an editable element (e.g. a
  // text field). This allows users to use spellcheck and other text-editing
  // features in text fields, but hides the menu otherwise.
  return !params.is_editable;
}

void OmniboxAimPopupWebUIContent::ShowUI() {
  OmniboxPopupWebUIBaseContent::ShowUI();

  // Start each session with a clean slate. `OnClearCallback()` normally clears
  // this, but it never runs if the handler went away before `Clear()`.
  draft_applied_on_close_ = false;

  // Capture the web contents when UI is first shown.
  active_web_contents_ = location_bar()->GetWebContents()
                             ? location_bar()->GetWebContents()->GetWeakPtr()
                             : nullptr;

  auto* handler = popup_aim_handler();
  if (!handler) {
    return;
  }
  // Drop any draft left over from a close that didn't drain it.
  handler->clear_cached_draft_text();

  auto* web_contents = contents_wrapper()->web_contents();
  auto* browser_window = webui::GetBrowserWindowInterface(web_contents);
  std::unique_ptr<SearchboxContextData::Context> context;
  if (browser_window) {
    auto* context_data = SearchboxContextData::From(browser_window);
    context = context_data->TakePendingContext();
  }
  if (!context) {
    context = std::make_unique<SearchboxContextData::Context>();
  }
  // Synchronize the invocation source on any pre-existing/warm session handle
  // with this invocation (or reset to default).
  if (auto* webui_controller = contents_wrapper()->GetWebUIController()) {
    if (auto* session_handle =
            webui_controller->GetOrCreateContextualSessionHandle()) {
      session_handle->set_invocation_source(context->invocation_source.value_or(
          lens::LensOverlayInvocationSource::kOmniboxContextualQuery));
    }
  }
  // TODO (crbug.com/502961786): Fix flickering of previous text on a new
  // instance of composebox.
  if (!controller()->edit_model()->CurrentTextIsURL()) {
    context->text =
        base::UTF16ToUTF8(location_bar()->GetOmniboxView()->GetText());
  }
  handler->OnPopupShown(std::move(context));
}

OmniboxPopupAimHandler* OmniboxAimPopupWebUIContent::popup_aim_handler() {
  auto* webui_controller = contents_wrapper()->GetWebUIController();
  return webui_controller ? webui_controller->popup_aim_handler() : nullptr;
}

BEGIN_METADATA(OmniboxAimPopupWebUIContent)
END_METADATA
