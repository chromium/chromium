// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"

#include <string_view>

#include "base/functional/bind.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_tab_selection_listener.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
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
          GetWrappedWebContents()->GetRenderWidgetHostView()) {
    render_widget_host_view->EnableAutoResize(min_size, max_size);
  }
}

void OmniboxPopupWebUIBaseContent::CloseUI() {
  // If the popup state is not shown, don't take any action. Closing the UI
  // multiple times can result in incorrect state transitions from OnClose.
  if (!is_shown_) {
    return;
  }

  is_shown_ = false;

  // Update the popup state manager that the popup is closing.
  // LocationBarView is subscribed to state changes and will close the widget.
  controller()->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);
}

void OmniboxPopupWebUIBaseContent::ShowUI() {
  // This is a signal from the WebUIContentsWrapper::Host. We use this signal to
  // check if the renderer crashes. If the renderer process has crashed, reset
  // the content URL and create a new renderer.
  if (contents_wrapper_->web_contents() &&
      contents_wrapper_->web_contents()->IsCrashed()) {
    LoadContent();
  }
  SetWebContents(contents_wrapper_->web_contents());

  is_shown_ = true;
}

void OmniboxPopupWebUIBaseContent::ShowCustomContextMenu(
    gfx::Point point,
    std::unique_ptr<ui::MenuModel> menu_model) {
  ConvertPointToScreen(this, &point);
  context_menu_ = std::make_unique<OmniboxContextMenu>(
      GetWidget(), location_bar_view_->GetOmniboxPopupFileSelector(),
      location_bar_view_->GetOmniboxPopupAimPresenter()
          ->GetWebUIContent()
          ->GetWrappedWebContents(),
      base::BindRepeating(&OmniboxPopupWebUIBaseContent::OnMenuClosed,
                          base::Unretained(this)));
  context_menu_->RunMenuAt(point, ui::mojom::MenuSourceType::kMouse);
}

void OmniboxPopupWebUIBaseContent::ResizeDueToAutoResize(
    content::WebContents* source,
    const gfx::Size& new_size) {
  WebView::ResizeDueToAutoResize(source, new_size);
  if (GetVisible()) {
    popup_presenter_->OnContentHeightChanged(new_size.height());
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

void OmniboxPopupWebUIBaseContent::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Note: This is needed for voice search in the AIM popup.
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

void OmniboxPopupWebUIBaseContent::SetContentURL(std::string_view url) {
  content_url_ = GURL(url);
  LoadContent();
}

void OmniboxPopupWebUIBaseContent::LoadContent() {
  DCHECK(!content_url_.is_empty());
  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<OmniboxPopupUI>>(
      content_url_, location_bar_view_->profile(), IDS_TASK_MANAGER_OMNIBOX);
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  SetWebContents(contents_wrapper_->web_contents());
  extensions::SetViewType(contents_wrapper_->web_contents(),
                          extensions::mojom::ViewType::kComponent);
  webui::SetBrowserWindowInterface(contents_wrapper_->web_contents(),
                                   location_bar_view_->browser());

  tab_selection_listener_ = std::make_unique<OmniboxPopupTabSelectionListener>(
      weak_factory_.GetWeakPtr(),
      location_bar_view_->browser()->tab_strip_model());
  // Make the OmniboxController available to the OmniboxPopupUI.
  OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
  OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
      ->set_omnibox_controller(controller_);

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

void OmniboxPopupWebUIBaseContent::OnPopupHidden() {
  // This removes the content from being considered for rendering by the
  // compositor while the popup is closed. The content is re-inserted right
  // before the view is displayed. This has the effect of tossing out old,
  // stale content in order to eliminiate it from being briefly displayed
  // while the new content is rendered. This improves visual performance
  // by eliminating that jank and stutter.
  // Under the hood, this forces the contents to clear the SurfaceId to keep
  // the GPU from embedding the content. By not deleting the contents we keep
  // the renderer alive, so when it is re-displayed it is much faster.
  SetWebContents(nullptr);
}

content::WebContents* OmniboxPopupWebUIBaseContent::GetWrappedWebContents() {
  return contents_wrapper_->web_contents();
}

void OmniboxPopupWebUIBaseContent::OnMenuClosed() {
  std::move(context_menu_).reset();
}

BEGIN_METADATA(OmniboxPopupWebUIBaseContent)
END_METADATA
