// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"

#include <string_view>

#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
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

void OmniboxAimPopupWebUIContent::CloseUI() {
  // Update the model. LocationBarView which owns the parent widget is observing
  // OmniboxEditModel and is responsible for closing the widget.
  controller()->edit_model()->SetInAiMode(false);
}

BEGIN_METADATA(OmniboxAimPopupWebUIContent)
END_METADATA
