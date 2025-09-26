// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list_types.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

OmniboxPopupPresenter::OmniboxPopupPresenter(LocationBarView* location_bar_view,
                                             OmniboxController* controller)
    : views::WebView(location_bar_view->profile()),
      location_bar_view_(location_bar_view) {
  set_owned_by_client(OwnedByClientPassKey());

  // Make the OmniboxController available to the OmniboxPopupUI.
  OmniboxPopupWebContentsHelper::CreateForWebContents(GetWebContents());
  OmniboxPopupWebContentsHelper::FromWebContents(GetWebContents())
      ->set_omnibox_controller(controller);
  LoadInitialURL(GURL(chrome::kChromeUIOmniboxPopupURL));

  location_bar_observation_.Observe(location_bar_view_);

  widget_ =
      std::make_unique<ThemeCopyingWidget>(location_bar_view_->GetWidget());

  const views::Widget* parent_widget = location_bar_view_->GetWidget();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);

#if BUILDFLAG(IS_WIN)
  // On Windows use the software compositor to ensure that we don't block
  // the UI thread during command buffer creation. See http://crbug.com/125248
  params.force_software_compositing = true;
#endif
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = parent_widget->GetNativeView();
  params.context = parent_widget->GetNativeWindow();

  RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, widget_.get());
  widget_->Init(std::move(params));
  widget_->SetContentsView(
      std::make_unique<RoundedOmniboxResultsFrame>(this, location_bar_view_));
  // On Show(), the widget height can not be 0 or else the compositor thinks
  // the webview is hidden and will not calculate its preferred size.
  SetWidgetContentHeight(1);
}

OmniboxPopupPresenter::~OmniboxPopupPresenter() = default;

void OmniboxPopupPresenter::Show() {
  widget_->Show();
}

void OmniboxPopupPresenter::Hide() {
  // Only close if UI DevTools settings allow.
  if (widget_ && widget_->ShouldHandleNativeWidgetActivationChanged(false)) {
    widget_->Hide();
  }
}

bool OmniboxPopupPresenter::IsShown() const {
  return widget_ && widget_->IsVisible();
}

WebuiOmniboxHandler* OmniboxPopupPresenter::GetHandler() {
  const bool ready = IsHandlerReady();
  if (!requested_handler_) {
    // Only log on first access.
    requested_handler_ = true;
    base::UmaHistogramBoolean("Omnibox.WebUI.HandlerReadyOnFirstAccess", ready);
  }
  if (!ready) {
    return nullptr;
  }
  OmniboxPopupUI* omnibox_popup_ui = static_cast<OmniboxPopupUI*>(
      GetWebContents()->GetWebUI()->GetController());
  return omnibox_popup_ui->handler();
}

void OmniboxPopupPresenter::AddedToWidget() {
  views::WebView::AddedToWidget();
  const float corner_radius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kOmniboxExpandedRadius);
  gfx::RoundedCornersF rounded_corner_radii =
      gfx::RoundedCornersF(0, 0, corner_radius, corner_radius);
  holder()->SetCornerRadii(rounded_corner_radii);
}

void OmniboxPopupPresenter::SetWidgetContentHeight(int content_height) {
  if (widget_) {
    // The width is known, and is the basis for consistent web content rendering
    // so width is specified exactly; then only height adjusts dynamically.
    gfx::Rect widget_bounds = location_bar_view_->GetBoundsInScreen();
    widget_bounds.Inset(
        -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
    widget_bounds.set_height(widget_bounds.height() + content_height);
    widget_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
    widget_->SetBounds(widget_bounds);
  }
}

void OmniboxPopupPresenter::ResizeDueToAutoResize(content::WebContents* source,
                                                  const gfx::Size& new_size) {
  SetWidgetContentHeight(new_size.height());
}

void OmniboxPopupPresenter::OnViewBoundsChanged(View* observed_view) {
  CHECK(observed_view == location_bar_view_);
  const int width =
      location_bar_view_->width() +
      RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().width();
  gfx::Size min_size(width, 1);
  gfx::Size max_size(INT_MAX, INT_MAX);

  content::RenderWidgetHostView* render_widget_host_view =
      GetWebContents()->GetRenderWidgetHostView();
  if (render_widget_host_view) {
    render_widget_host_view->EnableAutoResize(min_size, max_size);
  }
}

bool OmniboxPopupPresenter::IsHandlerReady() {
  OmniboxPopupUI* omnibox_popup_ui = static_cast<OmniboxPopupUI*>(
      GetWebContents()->GetWebUI()->GetController());
  return omnibox_popup_ui->handler() &&
         omnibox_popup_ui->handler()->IsRemoteBound();
}

BEGIN_METADATA(OmniboxPopupPresenter)
END_METADATA
