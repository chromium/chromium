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
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"

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

void OmniboxAimPopupWebUIContent::OnWidgetClosed() {
  auto* handler = popup_aim_handler();
  if (handler) {
    handler->OnWidgetClosed();
  }
}

void OmniboxAimPopupWebUIContent::OnPageClosedWithInput(
    const std::string& input) {
  location_bar_view()->GetOmniboxView()->RevertAll();
  if (!input.empty()) {
    location_bar_view()->GetOmniboxView()->SetUserText(base::UTF8ToUTF16(input),
                                                       /*update_popup=*/false);
  }
}

void OmniboxAimPopupWebUIContent::CloseUI() {
  OmniboxPopupWebUIBaseContent::CloseUI();
}

void OmniboxAimPopupWebUIContent::ShowUI() {
  OmniboxPopupWebUIBaseContent::ShowUI();

  auto* handler = popup_aim_handler();
  if (!handler) {
    return;
  }

  auto* web_contents = contents_wrapper()->web_contents();
  auto* browser_window = webui::GetBrowserWindowInterface(web_contents);
  auto* context_data = browser_window->GetFeatures().searchbox_context_data();
  auto context = context_data->TakePendingContext();
  if (!context) {
    context = std::make_unique<SearchboxContextData::Context>();
  }
  if (!controller()->edit_model()->CurrentTextIsURL()) {
    context->text =
        base::UTF16ToUTF8(location_bar_view()->GetOmniboxView()->GetText());
  }
  handler->OnWidgetShown(std::move(context));
}

OmniboxPopupAimHandler* OmniboxAimPopupWebUIContent::popup_aim_handler() {
  auto* webui_controller = contents_wrapper()->GetWebUIController();
  if (!webui_controller) {
    return nullptr;
  }
  auto* omnibox_popup_ui = webui_controller->GetAs<OmniboxPopupUI>();
  return omnibox_popup_ui ? omnibox_popup_ui->popup_aim_handler() : nullptr;
}

BEGIN_METADATA(OmniboxAimPopupWebUIContent)
END_METADATA
