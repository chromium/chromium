// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view.h"

#include <algorithm>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_delegate.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsView,
                                      kMultiContentsViewElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsView,
                                      kStartContainerViewScrimElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(MultiContentsView,
                                      kEndContainerViewScrimElementId);

MultiContentsView::MultiContentsView(
    BrowserView* browser_view,
    std::unique_ptr<MultiContentsViewDelegate> delegate)
    : browser_view_(browser_view),
      delegate_(std::move(delegate)),
      start_contents_view_inset_(
          gfx::Insets(kSplitViewContentInset).set_top(0).set_right(0)),
      end_contents_view_inset_(
          gfx::Insets(kSplitViewContentInset).set_top(0).set_left(0)) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  contents_container_views_.push_back(
      AddChildView(std::make_unique<ContentsContainerView>(browser_view_)));
  contents_container_views_[0]
      ->GetContentsView()
      ->set_is_primary_web_contents_for_window(true);
  contents_container_views_[0]->GetScrimView()->SetProperty(
      views::kElementIdentifierKey, kStartContainerViewScrimElementId);

  resize_area_ = AddChildView(std::make_unique<MultiContentsResizeArea>(this));
  resize_area_->SetVisible(false);

  contents_container_views_.push_back(
      AddChildView(std::make_unique<ContentsContainerView>(browser_view_)));
  contents_container_views_[1]->SetVisible(false);
  contents_container_views_[1]->GetScrimView()->SetProperty(
      views::kElementIdentifierKey, kEndContainerViewScrimElementId);

  for (auto* contents_container_view : contents_container_views_) {
    web_contents_focused_subscriptions_.push_back(
        contents_container_view->GetContentsView()
            ->AddWebContentsFocusedCallback(
                base::BindRepeating(&MultiContentsView::OnWebContentsFocused,
                                    base::Unretained(this))));
  }

  SetProperty(views::kElementIdentifierKey, kMultiContentsViewElementId);

  drop_target_view_ =
      AddChildView(std::make_unique<MultiContentsDropTargetView>(*delegate_));
  drop_target_controller_ =
      std::make_unique<MultiContentsViewDropTargetController>(
          *drop_target_view_);
}

MultiContentsView::~MultiContentsView() {
  drop_target_controller_.reset();
  drop_target_view_ = nullptr;
  resize_area_ = nullptr;
  RemoveAllChildViews();
}

ContentsWebView* MultiContentsView::GetActiveContentsView() {
  return contents_container_views_[active_index_]->GetContentsView();
}

ContentsWebView* MultiContentsView::GetInactiveContentsView() {
  return contents_container_views_[GetInactiveIndex()]->GetContentsView();
}

bool MultiContentsView::IsInSplitView() const {
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

void MultiContentsView::UpdateSplitRatio(double ratio) {
  if (start_ratio_ == ratio) {
    return;
  }

  start_ratio_ = ratio;
  InvalidateLayout();
}

void MultiContentsView::SetInactiveScrimVisibility(bool show_inactive_scrim) {
  if (show_inactive_scrim_ != show_inactive_scrim) {
    show_inactive_scrim_ = show_inactive_scrim;
    UpdateContentsBorderAndOverlay();
  }
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
  delegate_->ReverseWebContents();
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
  delegate_->ResizeWebContents(start_ratio);

  if (done_resizing) {
    initial_start_width_on_resize_ = std::nullopt;
  }
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
      delegate_->WebContentsFocused(web_view->web_contents());
    }
  }
}

// TODO(crbug.com/397777917): Consider using FlexSpecification weights and
// interior margins instead of a custom layout once this bug is resolved.
views::ProposedLayout MultiContentsView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }

  int height = size_bounds.height().value();
  int width = size_bounds.width().value();

  const gfx::Rect available_space(width, height);
  ViewWidths widths = GetViewWidths(available_space);

  gfx::Rect drop_target_rect(widths.drop_target_width,
                             available_space.height());
  gfx::Rect start_rect(available_space.origin(),
                       gfx::Size(widths.start_width, available_space.height()));
  gfx::Rect resize_rect(
      start_rect.top_right(),
      gfx::Size(widths.resize_width, available_space.height()));
  gfx::Rect end_rect(resize_rect.top_right(),
                     gfx::Size(widths.end_width, available_space.height()));

  if (drop_target_view_->side().has_value()) {
    switch (drop_target_view_->side().value()) {
      case MultiContentsDropTargetView::DropSide::START:
        // If the drop target view will show at the start, shift everything
        // over.
        start_rect.set_x(start_rect.x() + widths.drop_target_width);
        resize_rect.set_x(resize_rect.x() + widths.drop_target_width);
        end_rect.set_x(resize_rect.x() + widths.drop_target_width);
        drop_target_rect.set_origin(available_space.origin());
        break;
      case MultiContentsDropTargetView::DropSide::END:
        drop_target_rect.set_origin(end_rect.top_right());
        break;
      default:
        NOTREACHED();
    }
  }

  if (IsInSplitView()) {
    start_rect.Inset(start_contents_view_inset_);
    end_rect.Inset(end_contents_view_inset_);
  }

  layouts.child_layouts.emplace_back(contents_container_views_[0],
                                     contents_container_views_[0]->GetVisible(),
                                     start_rect);
  layouts.child_layouts.emplace_back(resize_area_.get(),
                                     resize_area_->GetVisible(), resize_rect);
  layouts.child_layouts.emplace_back(contents_container_views_[1],
                                     contents_container_views_[1]->GetVisible(),
                                     end_rect);

  layouts.child_layouts.emplace_back(drop_target_view_.get(),
                                     drop_target_view_->GetVisible(),
                                     drop_target_rect);

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

MultiContentsView::ViewWidths MultiContentsView::GetViewWidths(
    gfx::Rect available_space) const {
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
    widths.drop_target_width = drop_target_view_->GetPreferredWidth();

    // TODO(crbug.com/394369035): Drop targets currently don't scale with
    // browser size. Consider adding a min width value.
    widths.start_width = available_space.width() - widths.drop_target_width;
  }
  return ClampToMinWidth(widths);
}

MultiContentsView::ViewWidths MultiContentsView::ClampToMinWidth(
    ViewWidths widths) const {
  if (!IsInSplitView()) {
    // Don't clamp if in a single-view state, where other views should be 0
    // width.
    return widths;
  }

  const int min_percentage =
      kMinWebContentsWidthPercentage * browser_view_->GetBounds().width();
  const int min_fixed_value = min_contents_width_for_testing_.has_value()
                                  ? min_contents_width_for_testing_.value()
                                  : kMinWebContentsWidth;
  const int min_width = std::min(min_fixed_value, min_percentage);
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
  for (auto* contents_container_view : contents_container_views_) {
    const bool is_active =
        contents_container_view->GetContentsView() == GetActiveContentsView();
    contents_container_view->UpdateBorderAndOverlay(IsInSplitView(), is_active,
                                                    show_inactive_scrim_);
  }
}

BEGIN_METADATA(MultiContentsView)
END_METADATA
