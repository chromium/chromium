// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
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
  active_contents_view_ =
      AddChildView(std::make_unique<ContentsWebView>(browser_context));
  inactive_contents_view_ =
      AddChildView(std::make_unique<ContentsWebView>(browser_context));
  inactive_contents_view_->SetVisible(false);

  SetProperty(views::kElementIdentifierKey, kMultiContentsViewElementId);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded));
}

MultiContentsView::~MultiContentsView() = default;

void MultiContentsView::SetWebContents(content::WebContents* web_contents,
                                       bool active) {
  ContentsWebView* contents_view =
      active ? active_contents_view_ : inactive_contents_view_;
  contents_view->SetWebContents(web_contents);
  contents_view->SetVisible(web_contents != nullptr);
}

void MultiContentsView::SetActivePosition(int position) {
  // Position should never be less than 0 or equal to or greater than the total
  // number of contents views.
  CHECK(position >= 0 && position < 2);
  ReorderChildView(active_contents_view_, position);
}

bool MultiContentsView::PreHandleMouseEvent(const blink::WebMouseEvent& event) {
  if (event.GetTypeAsUiEventType() == ui::EventType::kMousePressed &&
      inactive_contents_view_->GetVisible()) {
    gfx::Rect inactive_contents_view_bounds =
        inactive_contents_view_->GetWebContents()->GetContainerBounds();
    const gfx::PointF& event_position = event.PositionInScreen();
    if (inactive_contents_view_bounds.Contains(event_position.x(),
                                               event_position.y())) {
      inactive_view_pressed_callback_.Run(
          inactive_contents_view_->GetWebContents());
    }
  }
  // Always allow the event to propagate to the WebContents, regardless of
  // whether it was also handled above.
  // TODO(crbug.com/394367683): Investigate why the click event isn't actually
  // making it to the WebContents. Likely this is because the WebContents is
  // being replaced as a result of activating the inactive tab.
  return false;
}

BEGIN_METADATA(MultiContentsView)
END_METADATA
