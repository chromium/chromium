// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/realbox/realbox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

OmniboxPopupPresenter::OmniboxPopupPresenter(LocationBarView* location_bar_view,
                                             OmniboxController* controller)
    : views::WebView(location_bar_view->profile()),
      location_bar_view_(location_bar_view),
      widget_(nullptr),
      requested_handler_(false) {
  set_owned_by_client();

  // Prepare for instantiation of a `RealboxHandler` that will connect with
  // this omnibox controller. The URL load will instantiate and bind
  // the handler asynchronously.
  OmniboxPopupUI::SetOmniboxController(controller);
  LoadInitialURL(GURL(chrome::kChromeUIOmniboxPopupURL));
  location_bar_view_->AddObserver(this);
}

OmniboxPopupPresenter::~OmniboxPopupPresenter() {
  location_bar_view_->RemoveObserver(this);
  ReleaseWidget(false);
}

void OmniboxPopupPresenter::Show() {
  if (!widget_) {
    widget_ = new ThemeCopyingWidget(location_bar_view_->GetWidget());

    views::Widget* parent_widget = location_bar_view_->GetWidget();
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
#if BUILDFLAG(IS_WIN)
    // On Windows use the software compositor to ensure that we don't block
    // the UI thread during command buffer creation. See http://crbug.com/125248
    params.force_software_compositing = true;
#endif
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.parent = parent_widget->GetNativeView();
    params.context = parent_widget->GetNativeWindow();

    RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, widget_);

    widget_->Init(std::move(params));

    widget_->ShowInactive();

    widget_->SetContentsView(
        std::make_unique<RoundedOmniboxResultsFrame>(this, location_bar_view_));
    widget_->AddObserver(this);
  }
  RealboxHandler* handler = GetHandler();
  if (handler && !handler->HasObserver(this)) {
    handler->AddObserver(this);
  }
}

void OmniboxPopupPresenter::Hide() {
  // Only close if UI DevTools settings allow.
  if (widget_ && widget_->ShouldHandleNativeWidgetActivationChanged(false)) {
    ReleaseWidget(true);
  }
}

bool OmniboxPopupPresenter::IsShown() const {
  return !!widget_;
}

RealboxHandler* OmniboxPopupPresenter::GetHandler() {
  bool ready = IsHandlerReady();
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

void OmniboxPopupPresenter::OnWidgetDestroyed(views::Widget* widget) {
  if (widget == widget_) {
    widget_ = nullptr;
  }
}

void OmniboxPopupPresenter::OnPopupElementSizeChanged(gfx::Size size) {
  webui_element_size_ = size;
  if (widget_) {
    // The width is known, and is the basis for consistent web content rendering
    // so width is specified exactly; then only height adjusts dynamically.
    gfx::Rect widget_bounds = location_bar_view_->GetBoundsInScreen();
    widget_bounds.Inset(
        -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());

    // TODO(crbug.com/1396174): Change max height according to max suggestion
    //  count and calculated row height, or use a more general maximum value.
    constexpr int kMaxHeight = 600;
    widget_bounds.set_height(widget_bounds.height() +
                             std::min(kMaxHeight, size.height()));
    widget_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
    widget_->SetBounds(widget_bounds);
  }
}

void OmniboxPopupPresenter::OnViewBoundsChanged(View* observed_view) {
  CHECK(observed_view == location_bar_view_);
  OnPopupElementSizeChanged(webui_element_size_);
}

bool OmniboxPopupPresenter::IsHandlerReady() {
  OmniboxPopupUI* omnibox_popup_ui = static_cast<OmniboxPopupUI*>(
      GetWebContents()->GetWebUI()->GetController());
  return omnibox_popup_ui->handler() &&
         omnibox_popup_ui->handler()->IsRemoteBound();
}

void OmniboxPopupPresenter::ReleaseWidget(bool close) {
  RealboxHandler* handler = GetHandler();
  if (handler && handler->HasObserver(this)) {
    handler->RemoveObserver(this);
  }
  if (widget_) {
    // Avoid possibility of dangling raw_ptr by nulling before cleanup.
    views::Widget* widget = widget_;
    widget_ = nullptr;

    widget->RemoveObserver(this);
    if (close) {
      widget->Close();
    }
  }
  CHECK(!views::WidgetObserver::IsInObserverList());
}

BEGIN_METADATA(OmniboxPopupPresenter, views::WebView)
END_METADATA
