// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"

#include <string_view>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

OmniboxPopupWebUIBaseContent::OmniboxPopupWebUIBaseContent(
    OmniboxPopupPresenterBase* presenter,
    LocationBarView* location_bar_view,
    OmniboxController* controller,
    bool top_rounded_corners)
    : views::WebView(location_bar_view->profile()),
      popup_presenter_(presenter),
      location_bar_view_(location_bar_view),
      controller_(controller),
      top_rounded_corners_(top_rounded_corners) {
  location_bar_view_->AddObserver(this);
}

OmniboxPopupWebUIBaseContent::~OmniboxPopupWebUIBaseContent() {
  location_bar_view_->RemoveObserver(this);
}

void OmniboxPopupWebUIBaseContent::AddedToWidget() {
  views::WebView::AddedToWidget();
  const float corner_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kOmniboxExpandedRadius);
  gfx::RoundedCornersF rounded_corner_radii = gfx::RoundedCornersF(
      top_rounded_corners_ ? corner_radius : 0,
      top_rounded_corners_ ? corner_radius : 0, corner_radius, corner_radius);
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

  OnViewBoundsChanged(location_bar_view_);
}

void OmniboxPopupWebUIBaseContent::OnViewBoundsChanged(
    views::View* observed_view) {
  CHECK(observed_view == location_bar_view_);
  const int width =
      location_bar_view_->width() +
      RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().width();
  gfx::Size min_size(width, 1);
  gfx::Size max_size(width, INT_MAX);
  if (auto* render_widget_host_view =
          GetWebContents()->GetRenderWidgetHostView()) {
    render_widget_host_view->EnableAutoResize(min_size, max_size);
  }
}

void OmniboxPopupWebUIBaseContent::CloseUI() {
  // Must implement this pure-virtual abstract function.
}

void OmniboxPopupWebUIBaseContent::ShowUI() {
  // Must implement this pure-virtual abstract function.
}

void OmniboxPopupWebUIBaseContent::ShowCustomContextMenu(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  ConvertPointToScreen(this, &point);
  context_menu_ = std::make_unique<OmniboxContextMenu>(
      GetWidget(), GetWebContents(),
      location_bar_view_->GetOmniboxPopupFileSelector());
  context_menu_->RunMenuAt(point, ui::mojom::MenuSourceType::kMouse);
}

void OmniboxPopupWebUIBaseContent::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& new_size) {
  if (GetVisible()) {
    popup_presenter_->SetWidgetContentHeight(new_size.height());
  }
}

bool OmniboxPopupWebUIBaseContent::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
      event.windows_key_code == ui::VKEY_ESCAPE) {
    return controller_->edit_model()->OnEscapeKeyPressed();
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void OmniboxPopupWebUIBaseContent::SetContentURL(std::string_view url) {
  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<OmniboxPopupUI>>(
      GURL(url), location_bar_view_->profile(), IDS_TASK_MANAGER_OMNIBOX);
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  SetWebContents(contents_wrapper_->web_contents());
  webui::SetBrowserWindowInterface(contents_wrapper_->web_contents(),
                                   location_bar_view_->browser());
  // Make the OmniboxController available to the OmniboxPopupUI.
  OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
  OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
      ->set_omnibox_controller(controller_);
}

BEGIN_METADATA(OmniboxPopupWebUIBaseContent)
END_METADATA
