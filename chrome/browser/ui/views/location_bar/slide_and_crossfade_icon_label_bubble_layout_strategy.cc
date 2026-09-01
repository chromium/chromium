// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/slide_and_crossfade_icon_label_bubble_layout_strategy.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"

namespace {

constexpr base::TimeDelta kExpandDuration = base::Milliseconds(333);
constexpr base::TimeDelta kCollapseDuration = base::Milliseconds(383);
constexpr double kFastFadeOutDurationMS = 50.0;

gfx::CubicBezier GetStandardDecelerationBezier() {
  return gfx::CubicBezier(0.2, 0.0, 0.0, 1.0);
}

}  // namespace

SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::
    SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy(
        IconLabelBubbleView& host)
    : IconLabelBubbleAnimationLayoutStrategy(host),
      trailing_image_view_(
          host.AddChildView(std::make_unique<views::ImageView>())) {
  CHECK(trailing_image_view_);
  host.label()->SetSkipSubpixelRenderingOpacityCheck(true);

  trailing_image_view_->SetCanProcessEventsWithinSubtree(false);
  trailing_image_view_->SetPaintToLayer();
  trailing_image_view_->layer()->SetFillsBoundsOpaquely(false);

  if (host.image_container_view()) {
    host.image_container_view()->SetPaintToLayer();
    host.image_container_view()->layer()->SetFillsBoundsOpaquely(false);
  }

  host.label()->SetVisible(true);
  host.ResetSlideAnimation(false);
}

SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::
    ~SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy() {
  // Clear the raw_ptr before removing the child view (which deletes it) to
  // prevent a DanglingRawPtrDetected crash.
  views::ImageView* view = trailing_image_view_;
  trailing_image_view_ = nullptr;
  host()->RemoveChildViewT(view);
  host()->label()->SetSkipSubpixelRenderingOpacityCheck(false);
  if (host()->image_container_view()) {
    host()->image_container_view()->DestroyLayer();
  }
}

views::ProposedLayout SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::
    CalculateProposedLayout(const views::SizeBounds& size_bounds,
                            const IconLabelBubbleView* host) const {
  views::ProposedLayout layout;
  layout.child_layouts.emplace_back(host->ink_drop_container(), true,
                                    host->GetLocalBounds());

  int preferred_width = GetPreferredWidth();
  int icon_size = host->image_container_view()->GetPreferredSize().width();
  const LayoutDimensions dimensions = GetLayoutDimensions();
  int min_width =
      dimensions.leading_expanded + icon_size + dimensions.trailing_expanded;

  int host_width = preferred_width;
  if (size_bounds.width().is_bounded() &&
      preferred_width > size_bounds.width().value()) {
    host_width = std::min(min_width, size_bounds.width().value());
  }
  // Calculate the preferred height by combining the icon container height
  // and the border padding. Bypassing views::View::CalculatePreferredSize here
  // is critical to prevent infinite recursion loop issues during startup.
  int host_height = host->image_container_view()->GetPreferredSize().height() +
                    host->GetInsets().height();
  if (size_bounds.height().is_bounded() &&
      host_height < size_bounds.height().value()) {
    host_height = size_bounds.height().value();
  }

  layout.host_size = gfx::Size(host_width, host_height);

  if (host_width <= min_width) {
    int x = (host_width - icon_size) / 2;
    gfx::Rect leading_icon_bounds(x, (host_height - icon_size) / 2, icon_size,
                                  icon_size);
    layout.child_layouts.emplace_back(
        const_cast<views::View*>(host->image_container_view()), true,
        leading_icon_bounds);
    layout.child_layouts.emplace_back(
        host->label(),
        static_cast<views::LayoutManagerBase*>(host->GetLayoutManager())
            ->CanBeVisible(host->label()),
        gfx::Rect());
    layout.child_layouts.emplace_back(trailing_image_view_.get(), false,
                                      gfx::Rect());
    if (host->separator_view()) {
      layout.child_layouts.emplace_back(
          const_cast<views::View*>(host->separator_view()), false, gfx::Rect());
    }
    return layout;
  }

  int label_width = host->label()->GetPreferredSize().width();
  int available_label_width =
      host_width - (dimensions.leading_expanded + icon_size +
                    dimensions.spacing_expanded + dimensions.trailing_expanded);
  if (label_width > available_label_width) {
    label_width = std::max(0, available_label_width);
  }

  // Resolve the current animation progress value (0.0 to 1.0).
  const double animation_value = host->GetAnimationValue();
  const double t_translation =
      host->IsAnimationShowing()
          ? GetStandardDecelerationBezier().Solve(animation_value)
          : 1.0 - GetStandardDecelerationBezier().Solve(1.0 - animation_value);

  // Target position for the label when fully expanded (leading icon hidden).
  int label_end_x = dimensions.leading_collapsed;
  gfx::Rect label_bounds(label_end_x, 0, label_width, host_height);

  gfx::Rect leading_bounds(dimensions.leading_expanded,
                           (host_height - icon_size) / 2, icon_size, icon_size);

  // Interpolate the label's sliding X-position between its collapsed position
  // (behind leading icon) and expanded position based on progress translation.
  int label_start_x =
      dimensions.leading_expanded + icon_size + dimensions.spacing_expanded;
  int label_x =
      gfx::Tween::IntValueBetween(t_translation, label_start_x, label_end_x);
  if (label_x + label_width > host_width - dimensions.trailing_expanded) {
    label_x = host_width - dimensions.trailing_expanded - label_width;
  }
  label_bounds.set_x(label_x);

  // The trailing icon remains stationary throughout the animation.
  int trailing_icon_x =
      dimensions.leading_collapsed + label_width + dimensions.spacing_collapsed;
  gfx::Rect trailing_bounds(trailing_icon_x, (host_height - icon_size) / 2,
                            icon_size, icon_size);

  layout.child_layouts.emplace_back(
      const_cast<views::View*>(host->image_container_view()), true,
      leading_bounds);
  layout.child_layouts.emplace_back(host->label(), label_width > 0,
                                    label_bounds);
  layout.child_layouts.emplace_back(trailing_image_view_.get(),
                                    animation_value > 0.0 && label_width > 0,
                                    trailing_bounds);
  if (host->separator_view()) {
    layout.child_layouts.emplace_back(
        const_cast<views::View*>(host->separator_view()), false, gfx::Rect());
  }

  return layout;
}

gfx::Size
SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::CalculatePreferredSize(
    const views::SizeBounds& available_size,
    const IconLabelBubbleView* host) const {
  int preferred_width = GetPreferredWidth();

  int preferred_height =
      host->image_container_view()->GetPreferredSize().height() +
      host->GetInsets().height();
  if (available_size.height().is_bounded() &&
      preferred_height < available_size.height().value()) {
    preferred_height = available_size.height().value();
  }

  return gfx::Size(preferred_width, preferred_height);
}

void SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::
    UpdateAnimationProgress(IconLabelBubbleView* host, double progress) {
  int preferred_width = GetPreferredWidth();

  bool is_constrained = host->width() > 0 && host->width() < preferred_width;
  double value = is_constrained ? 0.0 : progress;

  const bool animating_in = host->IsAnimationShowing();

  // Opacity initial values.
  float leading_icon_opacity = 1.0f;
  float trailing_icon_opacity = 0.0f;

  if (animating_in) {
    // FADE IN (EXPANDING)
    // The leading icon fades out quickly at the start of expansion (the first
    // 50ms) to prevent overlapping with the expanding label.
    const double expand_duration_ms = kExpandDuration.InMillisecondsF();
    const double expand_fade_out_fraction =
        kFastFadeOutDurationMS / expand_duration_ms;
    if (value < expand_fade_out_fraction) {
      leading_icon_opacity =
          1.0f - static_cast<float>(value / expand_fade_out_fraction);
    } else {
      leading_icon_opacity = 0.0f;
    }
    // The trailing icon fades in smoothly matching the cubic bezier layout.
    trailing_icon_opacity =
        static_cast<float>(GetStandardDecelerationBezier().Solve(value));
  } else {
    // FADE OUT (COLLAPSING)
    const double collapse_progress = 1.0 - value;
    const double collapse_duration_ms = kCollapseDuration.InMillisecondsF();

    // The trailing icon fades out quickly at the start of collapse (the first
    // 50ms).
    const double collapse_fade_out_fraction =
        kFastFadeOutDurationMS / collapse_duration_ms;
    if (collapse_progress < collapse_fade_out_fraction) {
      trailing_icon_opacity =
          1.0f -
          static_cast<float>(collapse_progress / collapse_fade_out_fraction);
    } else {
      trailing_icon_opacity = 0.0f;
    }

    // The leading icon fades in smoothly over the full duration.
    leading_icon_opacity = static_cast<float>(
        GetStandardDecelerationBezier().Solve(collapse_progress));
  }

  // Update compositor layers. We apply opacity to the GPU layer directly.
  if (host->image_container_view() && host->image_container_view()->layer()) {
    host->image_container_view()->layer()->SetOpacity(leading_icon_opacity);
  }

  if (trailing_image_view_->layer()) {
    trailing_image_view_->layer()->SetOpacity(trailing_icon_opacity);
  }
}

void SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::OnLayerAdded(
    IconLabelBubbleView* host,
    ui::Layer* layer) {
  UpdateAnimationProgress(host, host->GetAnimationValue());
}

void SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::OnLayerRemoved(
    IconLabelBubbleView* host,
    ui::Layer* layer) {
  if (host->image_container_view() && !host->image_container_view()->layer()) {
    host->image_container_view()->SetPaintToLayer();
    host->image_container_view()->layer()->SetFillsBoundsOpaquely(false);
  }
  UpdateAnimationProgress(host, host->GetAnimationValue());
}

std::optional<base::TimeDelta>
SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::GetAnimationDuration(
    bool show) const {
  return show ? kExpandDuration : kCollapseDuration;
}

void SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::SetupAnimation(
    gfx::SlideAnimation* animation,
    bool show) const {
  if (!animation->GetSlideDuration().is_zero()) {
    animation->SetSlideDuration(show ? kExpandDuration : kCollapseDuration);
  }
}

void SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::ResetAnimation(
    IconLabelBubbleView* host,
    bool show_label) {
  host->label()->SetVisible(true);
  UpdateAnimationProgress(host, show_label ? 1.0 : 0.0);
  host->PreferredSizeChanged();
}

void SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::OnAnimationEnded(
    IconLabelBubbleView* host) {
  host->label()->SetVisible(true);
  host->PreferredSizeChanged();
  host->UpdateBorder();
  views::InkDrop::Get(host)->GetInkDrop()->SetShowHighlightOnHover(true);
  views::InkDrop::Get(host)->GetInkDrop()->SetShowHighlightOnFocus(
      !views::FocusRing::Get(host));
  host->UpdateBackground();
}

bool SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::ShouldCollapse()
    const {
  const IconLabelBubbleView* host_view = host();
  if (host_view->width() == 0) {
    return false;
  }

  return host_view->width() < GetPreferredWidth();
}

int SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::GetPreferredWidth()
    const {
  const IconLabelBubbleView* host_view = host();
  int label_width = host_view->label()->GetPreferredSize().width();
  int icon_size = host_view->image_container_view()->GetPreferredSize().width();
  const LayoutDimensions dimensions = GetLayoutDimensions();
  return dimensions.leading_expanded + icon_size + dimensions.spacing_expanded +
         label_width + dimensions.trailing_expanded;
}

views::ImageView* SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::
    GetTrailingImageView() {
  return trailing_image_view_;
}

SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::LayoutDimensions
SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::GetLayoutDimensions()
    const {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  return touch_ui ? LayoutDimensions{10, 6, 14, 14, 6}
                  : LayoutDimensions{6, 4, 10, 10, 4};
}
