// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view.h"

#include <algorithm>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/status_bubble_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kMultiContentsViewDropTargetElementId);

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsView,
                                      kMultiContentsViewElementId);

MultiContentsView::MultiContentsView(
    BrowserView* browser_view,
    WebContentsFocusedCallback inactive_contents_focused_callback,
    WebContentsResizeCallback contents_resize_callback)
    : browser_view_(browser_view),
      inactive_contents_focused_callback_(inactive_contents_focused_callback),
      contents_resize_callback_(contents_resize_callback) {
  contents_container_views_.push_back(
      AddChildView(std::make_unique<ContentsContainerView>(browser_view_)));
  contents_container_views_[0]
      ->GetContentsView()
      ->set_is_primary_web_contents_for_window(true);

  resize_area_ = AddChildView(std::make_unique<MultiContentsResizeArea>(this));
  resize_area_->SetVisible(false);

  contents_container_views_.push_back(
      AddChildView(std::make_unique<ContentsContainerView>(browser_view_)));
  contents_container_views_[1]->SetVisible(false);

  for (auto* contents_container_view : contents_container_views_) {
    web_contents_focused_subscriptions_.push_back(
        contents_container_view->GetContentsView()
            ->AddWebContentsFocusedCallback(
                base::BindRepeating(&MultiContentsView::OnWebContentsFocused,
                                    base::Unretained(this))));
  }

  SetProperty(views::kElementIdentifierKey, kMultiContentsViewElementId);

  views::View* drop_target_view = AddChildView(std::make_unique<views::View>());
  drop_target_view->SetProperty(views::kElementIdentifierKey,
                                kMultiContentsViewDropTargetElementId);
  drop_target_view->SetVisible(false);
  drop_target_controller_ =
      std::make_unique<MultiContentsViewDropTargetController>(
          *drop_target_view);
}

MultiContentsView::~MultiContentsView() = default;

ContentsWebView* MultiContentsView::GetActiveContentsView() {
  return contents_container_views_[active_index_]->GetContentsView();
}

ContentsWebView* MultiContentsView::GetInactiveContentsView() {
  return contents_container_views_[GetInactiveIndex()]->GetContentsView();
}

bool MultiContentsView::IsInSplitView() {
  return resize_area_->GetVisible();
}

void MultiContentsView::SetWebContentsAtIndex(
    content::WebContents* web_contents,
    int index) {
  CHECK(index >= 0 && index < 2);
  contents_container_views_[index]->GetContentsView()->SetWebContents(
      web_contents);

  if (index == 1 && !contents_container_views_[1]->GetVisible()) {
    contents_container_views_[1]->SetVisible(true);
    resize_area_->SetVisible(true);
    UpdateContentsBorderAndOverlay();
  }
}

void MultiContentsView::CloseSplitView() {
  if (!IsInSplitView()) {
    return;
  }
  if (active_index_ == 1) {
    // Move the active WebContents so that the first ContentsContainerView in
    // contents_container_views_ can always be visible.
    std::iter_swap(contents_container_views_.begin(),
                   contents_container_views_.begin() + active_index_);
    active_index_ = 0;
  }
  contents_container_views_[1]->GetContentsView()->SetWebContents(nullptr);
  contents_container_views_[1]->SetVisible(false);
  resize_area_->SetVisible(false);
  UpdateContentsBorderAndOverlay();
}

void MultiContentsView::SetActiveIndex(int index) {
  // Index should never be less than 0 or equal to or greater than the total
  // number of contents views.
  CHECK(index >= 0 && index < 2);
  // We will only activate a visible contents view.
  CHECK(contents_container_views_[index]->GetVisible());
  active_index_ = index;
  GetActiveContentsView()->set_is_primary_web_contents_for_window(true);
  GetInactiveContentsView()->set_is_primary_web_contents_for_window(false);
  UpdateContentsBorderAndOverlay();
}

bool MultiContentsView::PreHandleMouseEvent(const blink::WebMouseEvent& event) {
  // Always allow the event to propagate to the WebContents, regardless of
  // whether it was also handled above.
  return false;
}

void MultiContentsView::ExecuteOnEachVisibleContentsView(
    base::RepeatingCallback<void(ContentsWebView*)> callback) {
  for (auto* contents_container_view : contents_container_views_) {
    if (contents_container_view->GetVisible()) {
      callback.Run(contents_container_view->GetContentsView());
    }
  }
}

void MultiContentsView::OnSwap() {
  CHECK(IsInSplitView());
  browser_view_->ReverseWebContents();
}

void MultiContentsView::UpdateSplitRatio(double ratio) {
  if (start_ratio_ == ratio) {
    return;
  }

  start_ratio_ = ratio;
  InvalidateLayout();
}

void MultiContentsView::OnResize(int resize_amount, bool done_resizing) {
  if (!initial_start_width_on_resize_.has_value()) {
    initial_start_width_on_resize_ =
        std::make_optional(contents_container_views_[0]->size().width());
  }

  double total_width = contents_container_views_[0]->size().width() +
                       contents_container_views_[0]->GetInsets().width() +
                       contents_container_views_[1]->size().width() +
                       contents_container_views_[1]->GetInsets().width();
  double start_ratio = (initial_start_width_on_resize_.value() +
                        contents_container_views_[0]->GetInsets().width() +
                        static_cast<double>(resize_amount)) /
                       total_width;
  contents_resize_callback_.Run(start_ratio);

  if (done_resizing) {
    initial_start_width_on_resize_ = std::nullopt;
  }
}

// TODO(crbug.com/397777917): Consider using FlexSpecification weights and
// interior margins instead of overriding layout once this bug is resolved.
void MultiContentsView::Layout(PassKey) {
  const gfx::Rect available_space(GetContentsBounds());
  ViewWidths widths = GetViewWidths(available_space);
  gfx::Rect start_rect(available_space.origin(),
                       gfx::Size(widths.start_width, available_space.height()));
  const gfx::Rect resize_rect(
      start_rect.top_right(),
      gfx::Size(widths.resize_width, available_space.height()));
  gfx::Rect end_rect(resize_rect.top_right(),
                     gfx::Size(widths.end_width, available_space.height()));
  float corner_radius = 0;
  if (IsInSplitView()) {
    start_rect.Inset(gfx::Insets(kSplitViewContentInset).set_right(0));
    end_rect.Inset(gfx::Insets(kSplitViewContentInset).set_left(0));
    corner_radius = kContentCornerRadius;
  }
  for (auto* contents_container_view : contents_container_views_) {
    contents_container_view->GetContentsView()->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF{corner_radius});
  }
  contents_container_views_[0]->SetBoundsRect(start_rect);
  resize_area_->SetBoundsRect(resize_rect);
  contents_container_views_[1]->SetBoundsRect(end_rect);
}

void MultiContentsView::OnPaint(gfx::Canvas* canvas) {
  // Paint the multi contents area background to match the toolbar.
  TopContainerBackground::PaintBackground(canvas, this, browser_view_);
}

void MultiContentsView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateContentsBorderAndOverlay();
}

int MultiContentsView::GetInactiveIndex() {
  return active_index_ == 0 ? 1 : 0;
}

void MultiContentsView::OnWebContentsFocused(views::WebView* web_view) {
  if (IsInSplitView()) {
    // Check whether the widget is visible as otherwise during browser hide,
    // inactive web contents gets focus. See crbug.com/419335827
    if (GetInactiveContentsView()->web_contents() == web_view->web_contents() &&
        GetWidget()->IsVisible()) {
      inactive_contents_focused_callback_.Run(web_view->web_contents());
    }
  }
}

MultiContentsView::ContentsContainerView::ContentsContainerView(
    BrowserView* browser_view) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  contents_view_ = AddChildView(
      std::make_unique<ContentsWebView>(browser_view->GetProfile()));
  mini_toolbar_ = AddChildView(std::make_unique<MultiContentsViewMiniToolbar>(
      browser_view, contents_view_));
}

views::ProposedLayout
MultiContentsView::ContentsContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }

  int height = size_bounds.height().value();
  int width = size_bounds.width().value();

  // |contents_view_| should fill the contents bounds.
  gfx::Rect contents_rect = GetContentsBounds();
  layouts.child_layouts.emplace_back(
      contents_view_.get(), contents_view_->GetVisible(), contents_rect);

  // |mini_toolbar_| should be offset in the bottom right corner, overlapping
  // the outline.
  gfx::Size mini_toolbar_size = mini_toolbar_->GetPreferredSize(
      views::SizeBounds(width - kContentOutlineCornerRadius, height));
  const int offset_x =
      width - mini_toolbar_size.width() + (kContentOutlineThickness / 2.0f);
  const int offset_y =
      height - mini_toolbar_size.height() + (kContentOutlineThickness / 2.0f);
  const gfx::Rect mini_toolbar_rect =
      gfx::Rect(offset_x, offset_y, mini_toolbar_size.width(),
                mini_toolbar_size.height());
  layouts.child_layouts.emplace_back(
      mini_toolbar_.get(), mini_toolbar_->GetVisible(), mini_toolbar_rect);

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}
BEGIN_METADATA(MultiContentsView, ContentsContainerView)
END_METADATA

MultiContentsView::ViewWidths MultiContentsView::GetViewWidths(
    gfx::Rect available_space) {
  ViewWidths widths;
  if (IsInSplitView()) {
    CHECK(contents_container_views_[0]->GetVisible() &&
          contents_container_views_[1]->GetVisible());
    widths.resize_width = resize_area_->GetPreferredSize().width();
    widths.start_width =
        start_ratio_ * (available_space.width() - widths.resize_width);
    widths.end_width =
        available_space.width() - widths.start_width - widths.resize_width;
  } else {
    CHECK(!contents_container_views_[1]->GetVisible());
    widths.start_width = available_space.width();
  }
  return ClampToMinWidth(widths);
}

MultiContentsView::ViewWidths MultiContentsView::ClampToMinWidth(
    ViewWidths widths) {
  const int min_percentage =
      kMinWebContentsWidthPercentage * browser_view_->GetBounds().width();
  const int min_fixed_value = min_contents_width_for_testing_.has_value()
                                  ? min_contents_width_for_testing_.value()
                                  : kMinWebContentsWidth;
  const int min_width = std::min(min_fixed_value, min_percentage);
  if (!IsInSplitView()) {
    // Don't clamp if in a single-view state, where other views should be 0
    // width.
    return widths;
  }
  if (widths.start_width < min_width) {
    const double diff = min_width - widths.start_width;
    widths.start_width += diff;
    widths.end_width -= diff;
  } else if (widths.end_width < min_width) {
    const double diff = min_width - widths.end_width;
    widths.end_width += diff;
    widths.start_width -= diff;
  }
  return widths;
}

void MultiContentsView::UpdateContentsBorderAndOverlay() {
  if (!IsInSplitView()) {
    for (auto* contents_container_view : contents_container_views_) {
      if (contents_container_view->GetBorder()) {
        contents_container_view->SetBorder(nullptr);
      }
    }
    // Update mini toolbar visibility.
    for (auto* contents_container_view : contents_container_views_) {
      contents_container_view->GetMiniToolbar()->SetVisible(false);
    }
    return;
  }

  // Draw active/inactive outlines around the contents areas and updates mini
  // toolbar visibility.
  const auto set_contents_border_and_mini_toolbar =
      [this](ContentsContainerView* contents_container_view) {
        const bool is_active = contents_container_view->GetContentsView() ==
                               GetActiveContentsView();
        const SkColor color =
            is_active ? GetColorProvider()->GetColor(
                            kColorMulitContentsViewActiveContentOutline)
                      : GetColorProvider()->GetColor(
                            kColorMulitContentsViewInactiveContentOutline);
        contents_container_view->SetBorder(views::CreatePaddedBorder(
            views::CreateRoundedRectBorder(kContentOutlineThickness,
                                           kContentOutlineCornerRadius, color),
            gfx::Insets(kSplitViewContentPadding)));
        // Mini toolbar should only be visible for the inactive contents
        // container view.
        contents_container_view->GetMiniToolbar()->SetVisible(!is_active);
      };
  for (auto* contents_container_view : contents_container_views_) {
    set_contents_border_and_mini_toolbar(contents_container_view);
  }
}

BEGIN_METADATA(MultiContentsView)
END_METADATA
