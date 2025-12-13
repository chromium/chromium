// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"

#include <string_view>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"

OmniboxPopupWebUIContent::OmniboxPopupWebUIContent(
    OmniboxPopupPresenterBase* presenter,
    LocationBarView* location_bar_view,
    OmniboxController* controller,
    bool include_location_bar_cutout,
    bool wants_focus)
    : OmniboxPopupWebUIBaseContent(
          presenter,
          location_bar_view,
          controller,
          /*top_rounded_corners=*/!include_location_bar_cutout),
      wants_focus_(wants_focus) {
  SetContentURL(chrome::kChromeUIOmniboxPopupURL);
}

OmniboxPopupWebUIContent::~OmniboxPopupWebUIContent() = default;

void OmniboxPopupWebUIContent::ShowUI() {
  OmniboxPopupWebUIBaseContent::ShowUI();

  if (auto* handler = omnibox_handler()) {
    handler->OnShow();
  }
}

WebuiOmniboxHandler* OmniboxPopupWebUIContent::omnibox_handler() {
  auto* webui_controller = contents_wrapper()->GetWebUIController();
  return webui_controller ? webui_controller->omnibox_handler() : nullptr;
}

BEGIN_METADATA(OmniboxPopupWebUIContent)
END_METADATA
