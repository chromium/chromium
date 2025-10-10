// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list_types.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "omnibox_popup_webui_content.h"
#include "rounded_omnibox_results_frame.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view_utils.h"

OmniboxPopupPresenter::OmniboxPopupPresenter(LocationBarView* location_bar_view,
                                             OmniboxController* controller)
    : location_bar_view_(location_bar_view),
      include_location_bar_cutout_(
          !base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup)) {
  owned_omnibox_popup_webui_content_ =
      std::make_unique<OmniboxPopupWebUIContent>(
          this, location_bar_view_, controller, include_location_bar_cutout_);
  location_bar_view_->AddObserver(this);
}

OmniboxPopupPresenter::~OmniboxPopupPresenter() {
  location_bar_view_->RemoveObserver(this);
  ReleaseWidget();
}

void OmniboxPopupPresenter::Show() {
  if (!widget_) {
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

    if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup)) {
      params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    }

    RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, widget_.get());

    widget_->MakeCloseSynchronous(base::BindOnce(
        &OmniboxPopupPresenter::OnWidgetClosed, base::Unretained(this)));

    widget_->Init(std::move(params));
    widget_->SetContentsView(std::make_unique<RoundedOmniboxResultsFrame>(
        owned_omnibox_popup_webui_content_.release(), location_bar_view_,
        include_location_bar_cutout_));

    widget_->SetVisibilityChangedAnimationsEnabled(false);
    // The widget height can not be 0 or else the compositor thinks the webview
    // is hidden and will not calculate its preferred size.
    SetWidgetContentHeight(1);

    if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup)) {
      widget_->Show();
      if (auto* content = GetOmniboxPopupWebUIContent()) {
        content->RequestFocus();
      }
    } else {
      widget_->ShowInactive();
    }
  }
}

void OmniboxPopupPresenter::Hide() {
  // Only close if UI DevTools settings allow.
  if (widget_ && widget_->ShouldHandleNativeWidgetActivationChanged(false)) {
    ReleaseWidget();
  }
}

bool OmniboxPopupPresenter::IsShown() const {
  return !!widget_;
}

void OmniboxPopupPresenter::SetWidgetContentHeight(int content_height) {
  if (widget_) {
    // The width is known, and is the basis for consistent web content rendering
    // so width is specified exactly; then only height adjusts dynamically.
    gfx::Rect widget_bounds = location_bar_view_->GetBoundsInScreen();
    if (include_location_bar_cutout_) {
      widget_bounds.Inset(
          -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
      widget_bounds.set_height(widget_bounds.height() + content_height);
    } else {
      widget_bounds.set_height(content_height);
    }
    widget_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
    widget_->SetBounds(widget_bounds);
  }
}

void OmniboxPopupPresenter::OnViewBoundsChanged(views::View* observed_view) {
  CHECK(observed_view == location_bar_view_);
  if (auto* content = GetOmniboxPopupWebUIContent()) {
    const int width =
        location_bar_view_->width() +
        RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().width();
    gfx::Size min_size(width, 1);
    gfx::Size max_size(width, INT_MAX);

    content::RenderWidgetHostView* render_widget_host_view =
        content->GetWebContents()->GetRenderWidgetHostView();
    if (render_widget_host_view) {
      render_widget_host_view->EnableAutoResize(min_size, max_size);
    }
  }
}

void OmniboxPopupPresenter::OnWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  owned_omnibox_popup_webui_content_ = AsViewClass<OmniboxPopupWebUIContent>(
      AsViewClass<RoundedOmniboxResultsFrame>(widget_->GetContentsView())
          ->ExtractContents());
  widget_.reset();
}

void OmniboxPopupPresenter::ReleaseWidget() {
  if (widget_) {
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

OmniboxPopupWebUIContent* OmniboxPopupPresenter::GetOmniboxPopupWebUIContent() {
  if (widget_) {
    auto* frame = views::AsViewClass<RoundedOmniboxResultsFrame>(
        widget_->GetContentsView());
    if (frame) {
      return views::AsViewClass<OmniboxPopupWebUIContent>(frame->GetContents());
    }
  }

  if (owned_omnibox_popup_webui_content_) {
    return owned_omnibox_popup_webui_content_.get();
  }

  return nullptr;
}
