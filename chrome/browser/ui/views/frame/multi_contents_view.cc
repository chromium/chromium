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
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsView,
                                      kMultiContentsViewElementId);

MultiContentsView::MultiContentsView(
    content::BrowserContext* browser_context,
    WebContentsPressedCallback inactive_view_pressed_callback)
    : inactive_view_pressed_callback_(inactive_view_pressed_callback) {
  start_contents_view_ =
      AddChildView(std::make_unique<ContentsWebView>(browser_context));

  resize_area_ = AddChildView(std::make_unique<MultiContentsResizeArea>(this));
  resize_area_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  resize_area_->SetVisible(false);

  end_contents_view_ =
      AddChildView(std::make_unique<ContentsWebView>(browser_context));
  end_contents_view_->SetVisible(false);

  SetProperty(views::kElementIdentifierKey, kMultiContentsViewElementId);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded));
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

ContentsWebView* MultiContentsView::SetActivePosition(int position) {
  // Position should never be less than 0 or equal to or greater than the total
  // number of contents views.
  CHECK(position >= 0 && position < 2);
  active_position_ = position;
  return GetActiveContentsView();
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
  // TODO(crbug.com/393450761): Implement this.
}

BEGIN_METADATA(MultiContentsView)
END_METADATA
