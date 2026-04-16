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
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
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
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/widget/widget.h"

OmniboxAimPopupWebUIContent::OmniboxAimPopupWebUIContent(
    OmniboxPopupPresenterBase* presenter,
    LocationBarView* location_bar_view,
    OmniboxController* controller)
    : OmniboxPopupWebUIBaseContent(presenter,
                                   location_bar_view,
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

void OmniboxAimPopupWebUIContent::OnClearCallback(
    base::WeakPtr<content::WebContents> original_web_contents,
    const std::string& input) {
  // Now that the WebUI has painted, it is safe to detach and cleanup.
  Detach();

  // Check if tabs have switched due to an async event.
  // Navigation to another tab is an async process which leads to a race
  // condition with the cleanup of the omnibox aim webui popup and which
  // web contents is referenced by the omnibox_edit_model.
  if (location_bar()->GetWebContents() == original_web_contents.get()) {
    ApplyInputAndCleanup(input);
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
  location_bar()->GetOmniboxView()->RevertAll();
  if (!input.empty()) {
    location_bar()->GetOmniboxView()->SetUserText(base::UTF8ToUTF16(input),
                                                  /*update_popup=*/false);
  }
}

std::string_view OmniboxAimPopupWebUIContent::GetMetricPrefix() const {
  return "Omnibox.Popup.Aim";
}

void OmniboxAimPopupWebUIContent::UpdateLocationBarFocusForScreenReader() {
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

void OmniboxAimPopupWebUIContent::CloseUI() {
  OmniboxPopupWebUIBaseContent::CloseUI();
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

  // Capture the web contents when UI is first shown.
  active_web_contents_ = location_bar()->GetWebContents()
                             ? location_bar()->GetWebContents()->GetWeakPtr()
                             : nullptr;

  auto* handler = popup_aim_handler();
  if (!handler) {
    return;
  }

  auto* web_contents = contents_wrapper()->web_contents();
  auto* browser_window = webui::GetBrowserWindowInterface(web_contents);
  std::unique_ptr<SearchboxContextData::Context> context;
  if (browser_window) {
    auto* context_data = browser_window->GetFeatures().searchbox_context_data();
    context = context_data->TakePendingContext();
  }
  if (!context) {
    context = std::make_unique<SearchboxContextData::Context>();
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
