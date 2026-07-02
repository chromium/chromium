// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/slide_and_crossfade_icon_label_bubble_layout_strategy.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"

namespace {

constexpr base::TimeDelta kExpandDuration = base::Milliseconds(333);
constexpr base::TimeDelta kCollapseDuration = base::Milliseconds(350);

gfx::CubicBezier GetForwardBezier() {
  return gfx::CubicBezier(0.05, 0.7368, 0.1, 1.0);
}

gfx::CubicBezier GetBackwardBezier() {
  return gfx::CubicBezier(0.05, 0.735, 0.1, 1.0);
}

}  // namespace

SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::
    SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy(IconLabelBubbleView& host)
    : IconLabelBubbleAnimationLayoutStrategy(host),
      trailing_image_view_(host.AddChildView(std::make_unique<views::ImageView>())) {
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

views::ProposedLayout
SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::CalculateProposedLayout(
    const views::SizeBounds& size_bounds,
    const IconLabelBubbleView* host) const {
  views::ProposedLayout layout;
  layout.child_layouts.emplace_back(host->ink_drop_container(), true,
                                    host->GetLocalBounds());

  int label_width = host->label()->GetPreferredSize().width();
  int icon_size = host->image_container_view()->GetPreferredSize().width();
  const LayoutDimensions dimensions = GetLayoutDimensions();
  int preferred_width = dimensions.leading_expanded + icon_size +
                        dimensions.spacing_expanded + label_width +
                        dimensions.trailing_expanded;

  int host_width = preferred_width;
  if (size_bounds.width().is_bounded() &&
      host_width > size_bounds.width().value()) {
    host_width = size_bounds.width().value();
  }
  // Calculate the preferred height by combining the icon container height
  // and the border padding. Bypassing views::View::CalculatePreferredSize here
  // is critical to prevent infinite recursion loop issues during startup.
  int host_height =
      host->image_container_view()->GetPreferredSize().height() +
      host->GetInsets().height();
  if (size_bounds.height().is_bounded() &&
      host_height < size_bounds.height().value()) {
    host_height = size_bounds.height().value();
  }

  layout.host_size = gfx::Size(host_width, host_height);

  int min_width = dimensions.leading_expanded + icon_size + dimensions.trailing_expanded;

  if (host_width <= min_width) {
    int x = (host_width - icon_size) / 2;
    gfx::Rect leading_icon_bounds(x, (host_height - icon_size) / 2, icon_size,
                           icon_size);
    layout.child_layouts.emplace_back(
        const_cast<views::View*>(host->image_container_view()), true,
        leading_icon_bounds);
    layout.child_layouts.emplace_back(host->label(), false, gfx::Rect());
    layout.child_layouts.emplace_back(trailing_image_view_.get(), false,
                                      gfx::Rect());
    if (host->separator_view()) {
      layout.child_layouts.emplace_back(
          const_cast<views::View*>(host->separator_view()), false,
          gfx::Rect());
    }
    return layout;
  }

  int available_label_width =
      host_width - (dimensions.leading_expanded + icon_size +
                    dimensions.spacing_expanded + dimensions.trailing_expanded);
  if (label_width > available_label_width) {
    label_width = std::max(0, available_label_width);
  }

  // Resolve the current animation progress value (0.0 to 1.0). If there is
  // not enough horizontal room, we lock the animation progress to 0.0 (collapsed).
  bool is_constrained = size_bounds.width().is_bounded() &&
                        size_bounds.width().value() < preferred_width;
  double value = is_constrained ? 0.0 : host->GetAnimationValue();
  double t_translation = value;

  // Use a Cubic Bezier curve to map linear time progress to spatial coordinates,
  // creating a natural ease-out (fast start, slow landing) transition.
  if (host->IsAnimationShowing()) {
    t_translation = GetForwardBezier().Solve(value);
  } else {
    double b = 1.0 - value;
    t_translation = 1.0 - GetBackwardBezier().Solve(b);
  }

  // Initial bounds for elements in their collapsed/starting layouts.
  int label_end_x = dimensions.leading_collapsed;
  gfx::Rect label_bounds(label_end_x, 0, label_width, host_height);

  gfx::Rect leading_bounds(dimensions.leading_expanded,
                           (host_height - icon_size) / 2, icon_size,
                           icon_size);

  // Interpolate the label's sliding X-position between its start (fully expanded)
  // and end (fully collapsed) bounds based on progress translation.
  int label_start_x =
      dimensions.leading_expanded + icon_size + dimensions.spacing_expanded;
  int label_x =
      gfx::Tween::IntValueBetween(t_translation, label_start_x, label_end_x);
  if (label_x + label_width > host_width - dimensions.trailing_expanded) {
    label_x = host_width - dimensions.trailing_expanded - label_width;
  }
  label_bounds.set_x(label_x);

  // Interpolate the trailing icon's sliding X-position. It slides from leading to
  // trailing, appearing from behind the label as the capsule widens.
  int trailing_icon_end_x =
      dimensions.leading_collapsed + label_width + dimensions.spacing_collapsed;
  int trailing_icon_start_x = trailing_icon_end_x - icon_size;
  int trailing_icon_x =
      gfx::Tween::IntValueBetween(t_translation, trailing_icon_start_x, trailing_icon_end_x);
  gfx::Rect trailing_bounds(trailing_icon_x, (host_height - icon_size) / 2, icon_size,
                            icon_size);

  layout.child_layouts.emplace_back(
      const_cast<views::View*>(host->image_container_view()), true,
      leading_bounds);
  layout.child_layouts.emplace_back(host->label(), label_width > 0,
                                    label_bounds);
  layout.child_layouts.emplace_back(trailing_image_view_.get(),
                                    value > 0.0 && label_width > 0,
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
  int label_width = host->label()->GetPreferredSize().width();
  int icon_size = host->image_container_view()->GetPreferredSize().width();
  const LayoutDimensions dimensions = GetLayoutDimensions();
  int preferred_width = dimensions.leading_expanded + icon_size +
                        dimensions.spacing_expanded + label_width +
                        dimensions.trailing_expanded;

  int preferred_height =
      host->image_container_view()->GetPreferredSize().height() +
      host->GetInsets().height();
  if (available_size.height().is_bounded() &&
      preferred_height < available_size.height().value()) {
    preferred_height = available_size.height().value();
  }

  return gfx::Size(preferred_width, preferred_height);
}

void SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::UpdateAnimationProgress(
    IconLabelBubbleView* host,
    double progress) {
  int label_width = host->label()->GetPreferredSize().width();
  int icon_size = host->image_container_view()->GetPreferredSize().width();
  const LayoutDimensions dimensions = GetLayoutDimensions();
  int preferred_width = dimensions.leading_expanded + icon_size +
                        dimensions.spacing_expanded + label_width +
                        dimensions.trailing_expanded;

  bool is_constrained = host->width() > 0 && host->width() < preferred_width;
  double value = is_constrained ? 0.0 : progress;

  const bool animating_in = host->IsAnimationShowing();

  // Opacity & scaling initial values.
  float leading_icon_opacity = 1.0f;
  float leading_icon_scale = 1.0f;
  float trailing_icon_opacity = 0.0f;

  if (animating_in) {
    // FADE IN (EXPANDING)
    // The leading icon fades out quickly at the start of expansion (the first
    // 17ms) to prevent overlapping with the expanding label.
    constexpr double kExpandFadeOutDurationMS = 17.0;
    const double expand_duration_ms = kExpandDuration.InMillisecondsF();
    const double expand_fade_out_fraction =
        kExpandFadeOutDurationMS / expand_duration_ms;
    if (value < expand_fade_out_fraction) {
      leading_icon_opacity =
          1.0f - static_cast<float>(value / expand_fade_out_fraction);
    } else {
      leading_icon_opacity = 0.0f;
    }
    // The trailing icon fades in smoothly matching the cubic bezier layout.
    trailing_icon_opacity = static_cast<float>(GetForwardBezier().Solve(value));
  } else {
    // FADE OUT (COLLAPSING)
    double b = 1.0 - value;
    const double collapse_duration_ms = kCollapseDuration.InMillisecondsF();

    // The leading icon remains hidden at first, then fades back in near the end
    // of collapse.
    constexpr double kCollapseFadeInDelayMS = 50.0;
    constexpr double kCollapseFadeInDurationMS = 17.0;
    const double collapse_fade_in_delay_fraction =
        kCollapseFadeInDelayMS / collapse_duration_ms;
    const double collapse_fade_in_duration_fraction =
        kCollapseFadeInDurationMS / collapse_duration_ms;

    if (b < collapse_fade_in_delay_fraction) {
      leading_icon_opacity = 0.0f;
    } else if (b < (collapse_fade_in_delay_fraction +
                    collapse_fade_in_duration_fraction)) {
      leading_icon_opacity =
          static_cast<float>((b - collapse_fade_in_delay_fraction) /
                             collapse_fade_in_duration_fraction);
    } else {
      leading_icon_opacity = 1.0f;
    }

    // The leading icon scales up from 50% to 100% size using an elastic bezier curve.
    constexpr double kCollapseScaleDelayMS = 50.0;
    constexpr double kCollapseScaleDurationMS = 300.0;
    const double collapse_scale_delay_fraction =
        kCollapseScaleDelayMS / collapse_duration_ms;
    const double collapse_scale_duration_fraction =
        kCollapseScaleDurationMS / collapse_duration_ms;

    if (b < collapse_scale_delay_fraction) {
      leading_icon_scale = 0.5f;
    } else {
      double s = (b - collapse_scale_delay_fraction) /
                 collapse_scale_duration_fraction;
      s = std::min(s, 1.0);
      gfx::CubicBezier scale_bezier(0.05, 1.68, 0.1, 1.0);
      leading_icon_scale =
          0.5f + static_cast<float>(scale_bezier.Solve(s) * 0.5);
    }

    // The trailing icon fades out immediately during the first half of collapse.
    constexpr double kCollapseFadeOutDurationMS = 150.0;
    const double collapse_fade_out_duration_fraction =
        kCollapseFadeOutDurationMS / collapse_duration_ms;
    if (b < collapse_fade_out_duration_fraction) {
      trailing_icon_opacity =
          1.0f - static_cast<float>(b / collapse_fade_out_duration_fraction);
    } else {
      trailing_icon_opacity = 0.0f;
    }
  }

  // Update compositor layers. We apply opacity and scale transformations to
  // the GPU layer directly. This avoids CPU layout recalculations and delivers
  // smooth FPS rendering.
  if (host->image_container_view() && host->image_container_view()->layer()) {
    host->image_container_view()->layer()->SetOpacity(leading_icon_opacity);
    gfx::Transform transform;
    // Perform scaling centered on the center of the icon
    transform.Translate(icon_size / 2.0f, icon_size / 2.0f);
    transform.Scale(leading_icon_scale, leading_icon_scale);
    transform.Translate(-icon_size / 2.0f, -icon_size / 2.0f);
    host->image_container_view()->layer()->SetTransform(transform);
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
  if (host->image_container_view() &&
      !host->image_container_view()->layer()) {
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

views::ImageView*
SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::GetTrailingImageView() {
  return trailing_image_view_;
}

SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::LayoutDimensions
SlideAndCrossfadeIconLabelBubbleAnimationLayoutStrategy::GetLayoutDimensions() const {
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  return touch_ui ? LayoutDimensions{10, 6, 14, 14, 6}
                  : LayoutDimensions{6, 4, 10, 10, 4};
}
