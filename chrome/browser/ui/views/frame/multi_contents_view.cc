// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view.h"

#include <algorithm>
#include <cstdlib>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "chrome/browser/actor/ui/actor_overlay_web_view.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/browser/ui/views/frame/contents_rounded_corner.h"
#include "chrome/browser/ui/views/frame/contents_separator.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_background_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_resize_area.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_delegate.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/view_class_properties.h"

void MultiContentsView::ContentsSeparators::Reset() {
  top_separator = nullptr;
  leading_separator = nullptr;
  trailing_separator = nullptr;
  top_leading_rounded_corner = nullptr;
  top_trailing_rounded_corner = nullptr;
}

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
  SetProperty(views::kElementIdentifierKey, kMultiContentsViewElementId);

  background_view_ =
      AddChildView(std::make_unique<MultiContentsBackgroundView>(browser_view));

  contents_container_views_.push_back(
      AddChildView(std::make_unique<ContentsContainerView>(browser_view_)));
  contents_container_views_[0]
      ->contents_view()
      ->set_is_primary_web_contents_for_window(true);

  resize_area_ = AddChildView(std::make_unique<MultiContentsResizeArea>(this));
  resize_area_->SetVisible(false);

  contents_container_views_.push_back(
      AddChildView(std::make_unique<ContentsContainerView>(browser_view_)));
  contents_container_views_[1]->SetVisible(false);

  drop_target_view_ =
      AddChildView(std::make_unique<MultiContentsDropTargetView>());
  drop_target_controller_ =
      std::make_unique<MultiContentsViewDropTargetController>(
          *drop_target_view_, *delegate_, g_browser_process->local_state());

  contents_separators_.top_separator =
      AddChildView(ContentsSeparator::CreateLayerBasedContentsSeparator());
  contents_separators_.top_separator->SetProperty(
      views::kElementIdentifierKey, kContentsSeparatorTopEdgeElementId);

  contents_separators_.leading_separator =
      AddChildView(ContentsSeparator::CreateLayerBasedContentsSeparator());
  contents_separators_.leading_separator->SetProperty(
      views::kElementIdentifierKey, kContentsSeparatorLeadingEdgeElementId);

  contents_separators_.trailing_separator =
      AddChildView(ContentsSeparator::CreateLayerBasedContentsSeparator());
  contents_separators_.trailing_separator->SetProperty(
      views::kElementIdentifierKey, kContentsSeparatorTrailingEdgeElementId);

  contents_separators_.top_leading_rounded_corner =
      AddChildView(std::make_unique<ContentsRoundedCorner>(
          browser_view_, views::ShapeContextTokens::kContentSeparatorRadius,
          base::BindRepeating([]() { return base::i18n::IsRTL(); })));
  contents_separators_.top_leading_rounded_corner->SetProperty(
      views::kElementIdentifierKey,
      kContentsSeparatorLeadingTopCornerElementId);

  contents_separators_.top_trailing_rounded_corner =
      AddChildView(std::make_unique<ContentsRoundedCorner>(
          browser_view_, views::ShapeContextTokens::kContentSeparatorRadius,
          base::BindRepeating([]() { return !base::i18n::IsRTL(); })));
  contents_separators_.top_trailing_rounded_corner->SetProperty(
      views::kElementIdentifierKey,
      kContentsSeparatorTrailingTopCornerElementId);

  for (auto* contents_container_view : contents_container_views_) {
    web_contents_focused_subscriptions_.push_back(
        contents_container_view->contents_view()->AddWebContentsFocusedCallback(
            base::BindRepeating(&MultiContentsView::OnWebContentsFocused,
                                base::Unretained(this))));

    if (contents_container_view->new_tab_footer_view()) {
      ntp_footer_focused_subscriptions_.push_back(
          contents_container_view->new_tab_footer_view()
              ->AddWebContentsFocusedCallback(
                  base::BindRepeating(&MultiContentsView::OnNtpFooterFocused,
                                      base::Unretained(this))));
    }

    if (contents_container_view->actor_overlay_web_view()) {
      actor_overlay_focused_subscriptions_.push_back(
          contents_container_view->actor_overlay_web_view()
              ->AddWebContentsFocusedCallback(
                  base::BindRepeating(&MultiContentsView::OnActorOverlayFocused,
                                      base::Unretained(this))));
    }
  }

  is_drag_drop_pref_enabled_ =
      browser_view_->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kSplitViewDragAndDropEnabled);

  pref_change_registrar_.Init(browser_view_->GetProfile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSplitViewDragAndDropEnabled,
      base::BindRepeating(&MultiContentsView::OnDragAndDropPrefStateChange,
                          base::Unretained(this)));
}

MultiContentsView::~MultiContentsView() {
  if (drop_target_controller_) {
    drop_target_controller_.reset();
  }
  drop_target_view_ = nullptr;
  resize_area_ = nullptr;
  contents_separators_.Reset();
  background_view_ = nullptr;
  RemoveAllChildViews();
}

ContentsWebView* MultiContentsView::GetActiveContentsView() const {
  return GetActiveContentsContainerView()->contents_view();
}

ContentsWebView* MultiContentsView::GetInactiveContentsView() const {
  return GetInactiveContentsContainerView()->contents_view();
}

ContentsContainerView* MultiContentsView::GetActiveContentsContainerView()
    const {
  return contents_container_views_[active_index_];
}

ContentsContainerView* MultiContentsView::GetInactiveContentsContainerView()
    const {
  return contents_container_views_[GetInactiveIndex()];
}

const gfx::RoundedCornersF& MultiContentsView::background_radii() const {
  return background_view_->GetRoundedCorners();
}

void MultiContentsView::SetBackgroundRadii(const gfx::RoundedCornersF& radii) {
  background_view_->SetRoundedCorners(radii);
}

ContentsContainerView* MultiContentsView::GetContentsContainerViewFor(
    content::WebContents* web_contents) const {
  for (auto* container_view : contents_container_views_) {
    if (container_view->contents_view()->web_contents() == web_contents) {
      return container_view;
    }
  }
  return nullptr;
}

bool MultiContentsView::IsInSplitView() const {
  return resize_area_->GetVisible();
}

void MultiContentsView::SetWebContentsAtIndex(
    content::WebContents* web_contents,
    int index) {
  CHECK(index >= 0 && index < 2);
  contents_container_views_[index]->contents_view()->SetWebContents(
      web_contents);

  if (index == 1 && !contents_container_views_[1]->GetVisible()) {
    contents_container_views_[1]->SetVisible(true);
    resize_area_->SetVisible(true);
    UpdateContentsBorderAndOverlay();
  }
}

void MultiContentsView::ShowSplitView(double ratio) {
  if (!contents_container_views_[1]->GetVisible()) {
    // If split view is not visible, set the `start_ratio_` and update the view
    // visibility.
    start_ratio_ = ratio;
    contents_container_views_[1]->SetVisible(true);
    resize_area_->SetVisible(true);
    UpdateContentsBorderAndOverlay();
  } else if (start_ratio_ != ratio) {
    // If the split view is visible but ratio is changed, update the split
    // ratio.
    UpdateSplitRatio(ratio);
  }
  // Split view is visible and ratio is not changed, do nothing.
}

void MultiContentsView::CloseSplitView() {
  if (!IsInSplitView()) {
    return;
  }

  if (active_index_ != 0) {
    ContentsContainerView* start_view = contents_container_views_[0];
    ContentsContainerView* active_view =
        contents_container_views_[active_index_];

    // Move the active WebContents so that the first ContentsContainerView in
    // contents_container_views_ can always be visible.
    std::iter_swap(contents_container_views_.begin(),
                   contents_container_views_.begin() + active_index_);

    // Reorder the child views so that focus order will be consistent with
    // contents_container_views_.
    size_t start_view_child_index = GetIndexOf(start_view).value();
    size_t active_view_child_index = GetIndexOf(active_view).value();
    ReorderChildView(start_view, active_view_child_index);
    ReorderChildView(active_view, start_view_child_index);

    active_index_ = 0;
  }
  contents_container_views_[1]->contents_view()->SetWebContents(nullptr);
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

void MultiContentsView::SetHighlightActiveContentsView(bool is_highlighted) {
  if (active_contents_view_highlighted_ != is_highlighted) {
    active_contents_view_highlighted_ = is_highlighted;
    UpdateContentsBorderAndOverlay();
  }
}

void MultiContentsView::ExecuteOnEachVisibleContentsView(
    base::RepeatingCallback<void(ContentsWebView*)> callback) {
  for (auto* contents_container_view : contents_container_views_) {
    if (contents_container_view->GetVisible()) {
      callback.Run(contents_container_view->contents_view());
    }
  }
}

void MultiContentsView::OnSwap() {
  CHECK(IsInSplitView());
  delegate_->ReverseWebContents();
}

int MultiContentsView::GetMinViewWidth() const {
  if (!IsInSplitView()) {
    return 0;
  }

  const int min_percentage =
      kMinWebContentsWidthPercentage * browser_view_->GetBounds().width();
  const int min_fixed_value = min_contents_width_for_testing_.has_value()
                                  ? min_contents_width_for_testing_.value()
                                  : kMinWebContentsWidth;
  return std::min(min_fixed_value, min_percentage);
}

std::vector<views::View*> MultiContentsView::GetAccessiblePanes() {
  std::vector<views::View*> accessible_panes;
  for (auto* contents_container_view : contents_container_views_) {
    auto contents_accessible_panes =
        contents_container_view->GetAccessiblePanes();
    accessible_panes.insert(accessible_panes.end(),
                            contents_accessible_panes.begin(),
                            contents_accessible_panes.end());
  }
  return accessible_panes;
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
  double end_width = (initial_start_width_on_resize_.value() +
                      contents_container_views_[0]->GetInsets().width() +
                      static_cast<double>(resize_amount));

  // If end_width is within the snap point widths, update to the snap point.
  delegate_->ResizeWebContents(
      CalculateRatioWithSnapPoints(end_width, total_width), done_resizing);

  if (done_resizing) {
    initial_start_width_on_resize_ = std::nullopt;
  }
}

double MultiContentsView::CalculateRatioWithSnapPoints(
    double end_width,
    double total_width) const {
  for (const double& snap_point : snap_points_) {
    double dp_snap_point = snap_point * total_width;
    if (std::abs(dp_snap_point - end_width) <
        features::kSideBySideSnapDistance.Get()) {
      return snap_point;
    }
  }
  return end_width / total_width;
}

void MultiContentsView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateContentsBorderAndOverlay();
}

int MultiContentsView::GetInactiveIndex() const {
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

void MultiContentsView::OnActorOverlayFocused(views::WebView* web_view) {
  if (IsInSplitView() && GetWidget()->IsVisible()) {
    for (auto* contents_container_view : contents_container_views_) {
      if (contents_container_view->actor_overlay_web_view() &&
          contents_container_view->actor_overlay_web_view() == web_view &&
          GetInactiveContentsView() ==
              contents_container_view->contents_view()) {
        return delegate_->WebContentsFocused(
            GetInactiveContentsView()->web_contents());
      }
    }
  }
}

void MultiContentsView::OnNtpFooterFocused(views::WebView* web_view) {
  if (IsInSplitView() && GetWidget()->IsVisible()) {
    for (auto* contents_container_view : contents_container_views_) {
      if (contents_container_view->new_tab_footer_view() &&
          contents_container_view->new_tab_footer_view() == web_view &&
          GetInactiveContentsView() ==
              contents_container_view->contents_view()) {
        return delegate_->WebContentsFocused(
            GetInactiveContentsView()->web_contents());
      }
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
  const int width = size_bounds.width().value();
  const int height = size_bounds.height().value();

  gfx::Rect available_space = gfx::Rect(width, height);

  const bool show_background =
      drop_target_view_->GetVisible() || IsInSplitView();
  layouts.child_layouts.emplace_back(background_view_.get(), show_background,
                                     available_space);

  if (IsDragAndDropEnabled()) {
    available_space =
        CalculateDropTargetLayout(available_space, layouts.child_layouts);
  }

  available_space =
      CalculateSeparatorLayouts(available_space, layouts.child_layouts);

  ViewWidths widths = GetViewWidths(available_space);

  gfx::Rect start_rect(available_space.origin(),
                       gfx::Size(widths.start_width, available_space.height()));
  gfx::Rect resize_rect(
      start_rect.top_right(),
      gfx::Size(widths.resize_width, available_space.height()));
  gfx::Rect end_rect(resize_rect.top_right(),
                     gfx::Size(widths.end_width, available_space.height()));

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

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

gfx::Rect MultiContentsView::CalculateDropTargetLayout(
    const gfx::Rect& available_space,
    std::vector<views::ChildLayout>& child_layouts) const {
  CHECK(IsDragAndDropEnabled());
  if (!drop_target_view_->GetVisible()) {
    child_layouts.emplace_back(drop_target_view_.get(), false, gfx::Rect());
    return available_space;
  }

  const int drop_target_width =
      drop_target_view_->GetPreferredWidth(available_space.width());

  const int drop_target_x = (drop_target_view_->side() ==
                             MultiContentsDropTargetView::DropSide::START)
                                ? available_space.x()
                                : available_space.right() - drop_target_width;
  const int remaining_space_x =
      available_space.x() + ((drop_target_view_->side() ==
                              MultiContentsDropTargetView::DropSide::START)
                                 ? drop_target_width
                                 : 0);

  child_layouts.emplace_back(
      drop_target_view_.get(), true,
      gfx::Rect(drop_target_x, available_space.y(), drop_target_width,
                available_space.height()));

  return gfx::Rect(remaining_space_x, available_space.y(),
                   available_space.width() - drop_target_width,
                   available_space.height());
}

gfx::Rect MultiContentsView::CalculateSeparatorLayouts(
    const gfx::Rect& available_space,
    std::vector<views::ChildLayout>& child_layouts) const {
  if (IsInSplitView()) {
    child_layouts.emplace_back(contents_separators_.top_separator.get(), false,
                               gfx::Rect());
    child_layouts.emplace_back(contents_separators_.leading_separator.get(),
                               false, gfx::Rect());
    child_layouts.emplace_back(contents_separators_.trailing_separator.get(),
                               false, gfx::Rect());
    child_layouts.emplace_back(
        contents_separators_.top_leading_rounded_corner.get(), false,
        gfx::Rect());
    child_layouts.emplace_back(
        contents_separators_.top_trailing_rounded_corner.get(), false,
        gfx::Rect());
    return available_space;
  }

  const int width = available_space.width();
  const int height = available_space.height();

  const int separator_height =
      contents_separators_.should_show_top
          ? contents_separators_.top_separator->GetPreferredSize().height()
          : 0;
  child_layouts.emplace_back(
      contents_separators_.top_separator.get(),
      contents_separators_.should_show_top,
      gfx::Rect(available_space.origin(), {width, separator_height}));

  const bool should_show_leading =
      contents_separators_.should_show_leading ||
      (drop_target_view_->side() ==
       MultiContentsDropTargetView::DropSide::START);
  const int leading_separator_width =
      should_show_leading
          ? contents_separators_.leading_separator->GetPreferredSize().width()
          : 0;
  child_layouts.emplace_back(
      contents_separators_.leading_separator.get(), should_show_leading,
      gfx::Rect(available_space.origin(), {leading_separator_width, height}));

  const bool should_show_trailing =
      contents_separators_.should_show_trailing ||
      (drop_target_view_->side() == MultiContentsDropTargetView::DropSide::END);

  const int trailing_separator_width =
      should_show_trailing
          ? contents_separators_.trailing_separator->GetPreferredSize().width()
          : 0;
  child_layouts.emplace_back(
      contents_separators_.trailing_separator.get(), should_show_trailing,
      gfx::Rect(available_space.right() - trailing_separator_width,
                available_space.y(), trailing_separator_width, height));

  child_layouts.emplace_back(
      contents_separators_.top_leading_rounded_corner.get(),
      should_show_leading && contents_separators_.should_show_top,
      gfx::Rect(
          available_space.origin(),
          contents_separators_.top_leading_rounded_corner->GetPreferredSize()));

  child_layouts.emplace_back(
      contents_separators_.top_trailing_rounded_corner.get(),
      should_show_trailing && contents_separators_.should_show_top,
      gfx::Rect({available_space.right() -
                     contents_separators_.top_trailing_rounded_corner
                         ->GetPreferredSize()
                         .width(),
                 available_space.y()},
                contents_separators_.top_trailing_rounded_corner
                    ->GetPreferredSize()));

  return gfx::Rect(available_space.x() + leading_separator_width,
                   available_space.y() + separator_height,
                   width - trailing_separator_width - leading_separator_width,
                   height - separator_height);
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
    widths.start_width = available_space.width();
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

  const int min_width = GetMinViewWidth();
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
        contents_container_view->contents_view() == GetActiveContentsView();
    contents_container_view->UpdateBorderAndOverlay(
        IsInSplitView(), is_active,
        is_active && active_contents_view_highlighted_);
  }
}

MultiContentsViewDropTargetController&
MultiContentsView::drop_target_controller() const {
  CHECK(IsDragAndDropEnabled());
  return *drop_target_controller_;
}

bool MultiContentsView::IsDragAndDropEnabled() const {
  // Split view drag and drop is only supported on normal browser types.
  return browser_view_->GetIsNormalType() && is_drag_drop_pref_enabled_;
}

void MultiContentsView::OnDragAndDropPrefStateChange() {
  is_drag_drop_pref_enabled_ =
      browser_view_->GetProfile()->GetPrefs()->GetBoolean(
          prefs::kSplitViewDragAndDropEnabled);
  InvalidateLayout();
}

void MultiContentsView::SetShouldShowTopSeparator(bool should_show) {
  if (contents_separators_.should_show_top == should_show) {
    return;
  }
  contents_separators_.should_show_top = should_show;
  start_contents_view_inset_.set_top(
      should_show ? 0 : MultiContentsView::kSplitViewContentInset);
  end_contents_view_inset_.set_top(
      should_show ? 0 : MultiContentsView::kSplitViewContentInset);

  // This can be called during BrowserView layout, so protect against creating a
  // layout loop.
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

void MultiContentsView::SetShouldShowLeadingSeparator(bool should_show) {
  if (contents_separators_.should_show_leading == should_show) {
    return;
  }
  contents_separators_.should_show_leading = should_show;
  start_contents_view_inset_.set_left(
      should_show ? 0 : MultiContentsView::kSplitViewContentInset);

  // This can be called during BrowserView layout, so protect against creating a
  // layout loop.
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

void MultiContentsView::SetShouldShowTrailingSeparator(bool should_show) {
  if (contents_separators_.should_show_trailing == should_show) {
    return;
  }
  contents_separators_.should_show_trailing = should_show;
  end_contents_view_inset_.set_right(
      should_show ? 0 : MultiContentsView::kSplitViewContentInset);

  // This can be called during BrowserView layout, so protect against creating a
  // layout loop.
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

BEGIN_METADATA(MultiContentsView)
END_METADATA
