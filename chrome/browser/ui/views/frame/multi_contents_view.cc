// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/types/event_type.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace {
const int kMinWebContentsWidth = 20;
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsView,
                                      kMultiContentsViewElementId);

MultiContentsView::MultiContentsView(
    content::BrowserContext* browser_context,
    WebContentsPressedCallback inactive_view_pressed_callback)
    : inactive_view_pressed_callback_(inactive_view_pressed_callback) {
  start_contents_view_ =
      AddChildView(std::make_unique<ContentsWebView>(browser_context));
  start_contents_view_->set_is_primary_web_contents_for_window(true);

  resize_area_ = AddChildView(std::make_unique<MultiContentsResizeArea>(this));
  resize_area_->SetVisible(false);

  end_contents_view_ =
      AddChildView(std::make_unique<ContentsWebView>(browser_context));
  end_contents_view_->SetVisible(false);

  SetProperty(views::kElementIdentifierKey, kMultiContentsViewElementId);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
}

MultiContentsView::~MultiContentsView() = default;

ContentsWebView* MultiContentsView::GetActiveContentsView() {
  return active_position_ == 0 ? start_contents_view_ : end_contents_view_;
}

ContentsWebView* MultiContentsView::GetInactiveContentsView() {
  return active_position_ == 0 ? end_contents_view_ : start_contents_view_;
}

void MultiContentsView::SetWebContents(content::WebContents* web_contents,
                                       bool active) {
  ContentsWebView* contents_view =
      active ? GetActiveContentsView() : GetInactiveContentsView();
  contents_view->SetWebContents(web_contents);
  contents_view->SetVisible(web_contents != nullptr);

  if (start_contents_view_->GetVisible() && end_contents_view_->GetVisible()) {
    resize_area_->SetVisible(true);
  } else {
    resize_area_->SetVisible(false);
  }
}

void MultiContentsView::SetActivePosition(int position) {
  // Position should never be less than 0 or equal to or greater than the total
  // number of contents views.
  CHECK(position >= 0 && position < 2);
  active_position_ = position;
  GetActiveContentsView()->set_is_primary_web_contents_for_window(true);
  GetInactiveContentsView()->set_is_primary_web_contents_for_window(false);
}

bool MultiContentsView::PreHandleMouseEvent(const blink::WebMouseEvent& event) {
  ContentsWebView* inactive_contents_view = GetInactiveContentsView();
  if (event.GetTypeAsUiEventType() == ui::EventType::kMousePressed &&
      inactive_contents_view->GetVisible()) {
    gfx::Rect inactive_contents_view_bounds =
        inactive_contents_view->GetWebContents()->GetContainerBounds();
    const gfx::PointF& event_position = event.PositionInScreen();
    if (inactive_contents_view_bounds.Contains(event_position.x(),
                                               event_position.y())) {
      inactive_view_pressed_callback_.Run(
          inactive_contents_view->GetWebContents());
    }
  }
  // Always allow the event to propagate to the WebContents, regardless of
  // whether it was also handled above.
  return false;
}

void MultiContentsView::OnResize(int resize_amount, bool done_resizing) {
  if (!initial_start_width_on_resize_.has_value()) {
    initial_start_width_on_resize_ =
        std::make_optional(start_contents_view_->size().width());
  }
  double total_width =
      start_contents_view_->size().width() + end_contents_view_->size().width();
  start_ratio_ =
      (initial_start_width_on_resize_.value() + resize_amount) / total_width;
  if (done_resizing) {
    initial_start_width_on_resize_ = std::nullopt;
  }
  InvalidateLayout();
}

// TODO(crbug.com/397777917): Consider using FlexSpecification weights instead
// of overriding layout once this bug is resolved.
void MultiContentsView::Layout(PassKey) {
  const gfx::Rect available_space(GetContentsBounds());
  ViewWidths widths = GetViewWidths(available_space);
  const gfx::Rect start_rect(
      available_space.origin(),
      gfx::Size(widths.start_width, available_space.height()));
  const gfx::Rect resize_rect(
      start_rect.top_right(),
      gfx::Size(widths.resize_width, available_space.height()));
  const gfx::Rect end_rect(
      resize_rect.top_right(),
      gfx::Size(widths.end_width, available_space.height()));
  start_contents_view_->SetBoundsRect(start_rect);
  resize_area_->SetBoundsRect(resize_rect);
  end_contents_view_->SetBoundsRect(end_rect);
}

MultiContentsView::ViewWidths MultiContentsView::GetViewWidths(
    gfx::Rect available_space) {
  ViewWidths widths;
  if (resize_area_->GetVisible()) {
    CHECK(start_contents_view_->GetVisible() &&
          end_contents_view_->GetVisible());
    widths.resize_width = resize_area_->GetPreferredSize().width();
    widths.start_width =
        start_ratio_ * (available_space.width() - widths.resize_width);
    widths.end_width =
        available_space.width() - widths.start_width - widths.resize_width;
  } else if (start_contents_view_->GetVisible()) {
    CHECK(!end_contents_view_->GetVisible());
    widths.start_width = available_space.width();
  } else {
    CHECK(end_contents_view_->GetVisible());
    widths.end_width = available_space.width();
  }
  return ClampToMinWidth(widths);
}

MultiContentsView::ViewWidths MultiContentsView::ClampToMinWidth(
    ViewWidths widths) {
  if (!resize_area_->GetVisible()) {
    // Don't clamp if in a single-view state, where other views should be 0
    // width.
    return widths;
  }
  if (widths.start_width < kMinWebContentsWidth) {
    const double diff = kMinWebContentsWidth - widths.start_width;
    widths.start_width += diff;
    widths.end_width -= diff;
  } else if (widths.end_width < kMinWebContentsWidth) {
    const double diff = kMinWebContentsWidth - widths.end_width;
    widths.end_width += diff;
    widths.start_width -= diff;
  }
  return widths;
}

BEGIN_METADATA(MultiContentsView)
END_METADATA
