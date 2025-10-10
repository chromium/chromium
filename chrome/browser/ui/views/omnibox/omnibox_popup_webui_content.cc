// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/zoom/zoom_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

OmniboxPopupWebUIContent::OmniboxPopupWebUIContent(
    OmniboxPopupPresenter* presenter,
    LocationBarView* location_bar_view,
    OmniboxController* controller,
    bool include_location_bar_cutout)
    : views::WebView(location_bar_view->profile()),
      location_bar_view_(location_bar_view),
      omnibox_popup_presenter_(presenter),
      controller_(controller),
      include_location_bar_cutout_(include_location_bar_cutout) {
  // Make the OmniboxController available to the OmniboxPopupUI.
  OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
  OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
      ->set_omnibox_controller(controller);

  LoadInitialURL(GURL(chrome::kChromeUIOmniboxPopupURL));
}

OmniboxPopupWebUIContent::~OmniboxPopupWebUIContent() = default;

void OmniboxPopupWebUIContent::AddedToWidget() {
  views::WebView::AddedToWidget();
  const float corner_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kOmniboxExpandedRadius);
  gfx::RoundedCornersF rounded_corner_radii =
      gfx::RoundedCornersF(include_location_bar_cutout_ ? 0 : corner_radius,
                           include_location_bar_cutout_ ? 0 : corner_radius,
                           corner_radius, corner_radius);
  holder()->SetCornerRadii(rounded_corner_radii);

  // Manually set zoom level, since any zooming is undesirable in the omnibox.
  auto* zoom_controller =
      zoom::ZoomController::FromWebContents(GetWebContents());
  if (!zoom_controller) {
    // Create ZoomController manually, if not already exists, because it is
    // not automatically created when the WebUI has not been opened in a tab.
    zoom_controller =
        zoom::ZoomController::CreateForWebContents(GetWebContents());
  }
  zoom_controller->SetZoomMode(zoom::ZoomController::ZOOM_MODE_ISOLATED);
  zoom_controller->SetZoomLevel(0);
}

void OmniboxPopupWebUIContent::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& new_size) {
  omnibox_popup_presenter_->SetWidgetContentHeight(new_size.height());
}

bool OmniboxPopupWebUIContent::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
      event.windows_key_code == ui::VKEY_ESCAPE) {
    return controller_->edit_model()->OnEscapeKeyPressed();
  }
  return false;
}

BEGIN_METADATA(OmniboxPopupWebUIContent)
END_METADATA
