// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"

#include <optional>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "ui/views/view_utils.h"

OmniboxPopupPresenterBase::OmniboxPopupPresenterBase(
    LocationBarView* location_bar_view)
    : location_bar_view_(location_bar_view) {
  owned_omnibox_popup_webui_container_ =
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build();
}

OmniboxPopupPresenterBase::~OmniboxPopupPresenterBase() {
  ReleaseWidget();
}

void OmniboxPopupPresenterBase::Show(bool ai_mode) {
  bool widget_created = EnsureWidgetCreated();

  ShowWebUIContent(ai_mode ? 1 : 0);

  AsViewClass<RoundedOmniboxResultsFrame>(widget_->GetContentsView())
      ->SetCutoutVisibility(
          GetActivePopupWebUIContent()->include_location_bar_cutout());

  VLOG(4) << "widget_created = " << (widget_created ? "true" : "false");
  if (widget_created) {
    widget_->ShowInactive();

    if (ai_mode) {
      SetWidgetContentHeight(1);
    }

    if (auto* content = GetActivePopupWebUIContent()) {
      content->GetWebContents()->WasShown();
      VLOG(4) << "content->wants_focus() = "
              << (content->wants_focus() ? "true" : "false");
      if (content->wants_focus()) {
        widget_->Activate();
        content->RequestFocus();
      }
    }
  }
}

void OmniboxPopupPresenterBase::Hide() {
  // Only close if UI DevTools settings allow.
  if (widget_ && widget_->ShouldHandleNativeWidgetActivationChanged(false)) {
    ReleaseWidget();
  }
}

bool OmniboxPopupPresenterBase::IsShown() const {
  return !!widget_;
}

std::optional<size_t> OmniboxPopupPresenterBase::GetShowingWebUIContentIndex()
    const {
  return std::nullopt;
}

void OmniboxPopupPresenterBase::SetWidgetContentHeight(int content_height) {
  if (widget_) {
    // The width is known, and is the basis for consistent web content rendering
    // so width is specified exactly; then only height adjusts dynamically.
    gfx::Rect widget_bounds = location_bar_view_->GetBoundsInScreen();
    if (GetActivePopupWebUIContent()->include_location_bar_cutout()) {
      widget_bounds.Inset(
          -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
      widget_bounds.set_height(widget_bounds.height() + content_height);
    } else {
      widget_bounds.set_height(
          std::max(content_height, widget_bounds.height()));
    }
    widget_bounds.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
    widget_->SetBounds(widget_bounds);
  }
}

views::View* OmniboxPopupPresenterBase::GetOmniboxPopupWebUIContainer() const {
  if (owned_omnibox_popup_webui_container_) {
    return owned_omnibox_popup_webui_container_.get();
  }
  CHECK(widget_);
  return views::AsViewClass<RoundedOmniboxResultsFrame>(
             widget_->GetContentsView())
      ->GetContents();
}

OmniboxPopupWebUIContent*
OmniboxPopupPresenterBase::AddOmniboxPopupWebUIContent(
    OmniboxController* controller,
    std::string_view content_url,
    bool include_location_bar_cutout,
    bool wants_focus) {
  return GetOmniboxPopupWebUIContainer()->AddChildView(
      std::make_unique<OmniboxPopupWebUIContent>(
          this, location_bar_view_.get(), controller, content_url,
          include_location_bar_cutout, wants_focus));
}

OmniboxPopupWebUIContent*
OmniboxPopupPresenterBase::GetActivePopupWebUIContent() const {
  for (auto child : GetOmniboxPopupWebUIContainer()->children()) {
    if (child->GetVisible()) {
      return views::AsViewClass<OmniboxPopupWebUIContent>(child);
    }
  }
  NOTREACHED() << "No visible Web Contents";
}

bool OmniboxPopupPresenterBase::EnsureWidgetCreated() {
  if (widget_) {
    return false;
  }

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

  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup) ||
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxAimPopup)) {
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  }

  RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, widget_.get());

  widget_->MakeCloseSynchronous(base::BindOnce(
      &OmniboxPopupPresenterBase::OnWidgetClosed, base::Unretained(this)));

  widget_->Init(std::move(params));
  widget_->SetContentsView(std::make_unique<RoundedOmniboxResultsFrame>(
      owned_omnibox_popup_webui_container_.release(), location_bar_view_));

  widget_->SetVisibilityChangedAnimationsEnabled(false);
  return true;
}

void OmniboxPopupPresenterBase::WidgetDestroyed() {}

void OmniboxPopupPresenterBase::OnWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  owned_omnibox_popup_webui_container_ =
      AsViewClass<RoundedOmniboxResultsFrame>(widget_->GetContentsView())
          ->ExtractContents();
  widget_.reset();
  WidgetDestroyed();
}

void OmniboxPopupPresenterBase::ReleaseWidget() {
  if (widget_) {
    VLOG(4) << "ReleaseWidget()";
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}
