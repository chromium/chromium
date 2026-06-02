// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"

#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"

OmniboxFullPopupWebUIContent::OmniboxFullPopupWebUIContent(
    OmniboxPopupPresenterBase* presenter,
    LocationBar* location_bar,
    OmniboxController* controller)
    : OmniboxPopupWebUIContent(presenter,
                               location_bar,
                               controller,
                               /*include_location_bar_cutout=*/false,
                               /*wants_focus=*/true) {
  SetContentURL(chrome::kChromeUIOmniboxPopupURL);
}

OmniboxFullPopupWebUIContent::~OmniboxFullPopupWebUIContent() = default;

// TODO(b/504668887): If necessary, copy `OmniboxAimPopupWebUIContent::Clear()`
// implementation here to deal with tab state restoration issue(s) when the user
// creates a new tab while there's an in-progress text input in the popup.

std::string_view OmniboxFullPopupWebUIContent::GetMetricPrefix() const {
  return "Omnibox.Popup.FullWebUI";
}

// TODO(b/504669142): If necessary, copy
// `OmniboxAimPopupWebUIContent::UpdateLocationBarFocusForScreenReader()`
// implementation to here to deal with potential popup focus issue(s) when a
// screenreader is being used.

// Override of WebUIContentsWrapper::Host::HandleContextMenu. This mirrors
// content::WebContentsDelegate::HandleContextMenu, which is called by the
// WebContentsImpl to allow the delegate to handle the context menu if desired.
// Returning true means the context menu request was handled (and thus
// the caller suppresses their own context menu). Returning false allows
// the default context menu to be shown.
bool OmniboxFullPopupWebUIContent::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppress the context menu unless it's on an editable element (e.g. a
  // text field). This allows users to use spellcheck and other text-editing
  // features in text fields, but hides the menu otherwise.
  return !params.is_editable;
}

BEGIN_METADATA(OmniboxFullPopupWebUIContent)
END_METADATA
