// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/standard_icon_label_bubble_layout_strategy.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/proposed_layout.h"

StandardIconLabelBubbleAnimationLayoutStrategy::StandardIconLabelBubbleAnimationLayoutStrategy(
    IconLabelBubbleView& host)
    : IconLabelBubbleAnimationLayoutStrategy(host) {}

StandardIconLabelBubbleAnimationLayoutStrategy::~StandardIconLabelBubbleAnimationLayoutStrategy() =
    default;

views::ProposedLayout
StandardIconLabelBubbleAnimationLayoutStrategy::CalculateProposedLayout(
    const views::SizeBounds& size_bounds,
    const IconLabelBubbleView* host) const {
  views::ProposedLayout layout;
  layout.child_layouts.emplace_back(host->ink_drop_container(), true,
                                    host->GetLocalBounds());

  // Allows the label to be displayed even if there's not enough space with the
  // preferred size of this view, due to an animation being in progress.
  bool can_expand_label_for_animation = host->is_animating_label();

  // First calculate the preferred size of this view, according to the
  // preferred size of the label (i.e. with an expanded label).
  // If that won't fit in the available bounds, then size this view
  // based on a collapsed label.
  const int preferred_label_width = host->label()->GetPreferredSize().width();
  gfx::Size preferred_size = host->GetSizeForLabelWidth(preferred_label_width);
  if (size_bounds.width().is_bounded() &&
      preferred_size.width() > size_bounds.width()) {
    preferred_size = host->GetMinimumSize();
    if (host->ShouldShowLabel() &&
        host->label()->GetElideBehavior() != gfx::NO_ELIDE) {
      preferred_size.set_width(
          std::max(preferred_size.width(), size_bounds.width().value()));
    } else {
      can_expand_label_for_animation = false;
    }
  }
  if (size_bounds.height().is_bounded() &&
      preferred_size.height() < size_bounds.height()) {
    preferred_size.set_height(size_bounds.height().value());
  }
  layout.host_size = preferred_size;
  const int height = preferred_size.height();

  // We may not have horizontal room for both the image and the trailing
  // padding. When the view is expanding (or showing-label steady state), the
  // image. When the view is contracting (or hidden-label steady state), whittle
  // away at the trailing padding instead.
  int bubble_trailing_padding = host->GetEndPaddingWithSeparator();
  int image_width = host->image_container_view()->GetPreferredSize().width();
  const int space_shortage =
      image_width + bubble_trailing_padding - preferred_size.width();
  if (space_shortage > 0) {
    if (host->ShouldShowLabel()) {
      image_width -= space_shortage;
    } else {
      bubble_trailing_padding -= space_shortage;
    }
  }

  const int image_x = host->GetInsets().left();
  const gfx::Rect image_bounds(image_x, 0, image_width, height);

  // There may be extra padding added if the label is shown.
  // The "_with_label" image values are used to compute the label's bounds,
  // which is then compared to the available bounds for the label. If the
  // label would fit in the bounds (i.e. not collapsed), then the "_with_label"
  // values will accepted as the image's bounds too.
  const int image_x_with_label =
      image_x + host->GetWidthBetween(
                    0, host->expanded_label_additional_insets_.leading());
  const gfx::Rect image_bounds_with_label(image_x_with_label, 0, image_width,
                                          height);

  // Compute the label bounds. The label gets whatever size is left over after
  // accounting for the preferred image width and padding amounts. Note that if
  // the label has zero size it doesn't actually matter what we compute its X
  // value to be, since it won't be visible.
  const int label_x =
      image_bounds_with_label.right() + host->GetInternalSpacing();
  const int available_label_width = std::max(
      0, layout.host_size.width() - label_x - bubble_trailing_padding -
             host->GetWidthBetweenIconAndSeparator());
  gfx::Rect label_bounds(label_x, 0, available_label_width, height);
  layout.child_layouts.emplace_back(
      host->label(),
      static_cast<views::LayoutManagerBase*>(host->GetLayoutManager())
          ->CanBeVisible(host->label()),
      label_bounds);

  // If the fully expanded label fits, or it is mid-animation, then we
  // accept the "_with_label" image bounds.
  const bool can_label_expand = available_label_width >= preferred_label_width;
  const bool should_use_label_bounds =
      host->ShouldShowLabel() &&
      (can_label_expand || can_expand_label_for_animation);
  layout.child_layouts.emplace_back(
      const_cast<views::View*>(host->image_container_view()), true,
      should_use_label_bounds ? image_bounds_with_label : image_bounds);

  // The separator should be the same height as the icons.
  const int separator_height =
      GetLayoutConstant(LayoutConstant::kLocationBarIconSize);
  gfx::Rect separator_bounds(label_bounds);
  separator_bounds.Inset(
      gfx::Insets::VH((separator_bounds.height() - separator_height) / 2, 0));
  float separator_width = host->GetWidthBetweenIconAndSeparator() +
                          host->GetEndPaddingWithSeparator();
  int separator_x = host->label()->GetText().empty() ? image_bounds.right()
                                                     : label_bounds.right();

  layout.child_layouts.emplace_back(
      const_cast<views::View*>(host->separator_view()),
      host->ShouldShowSeparator(),
      gfx::Rect(separator_x, separator_bounds.y(), separator_width,
                separator_height));

  return layout;
}

gfx::Size StandardIconLabelBubbleAnimationLayoutStrategy::CalculatePreferredSize(
    const views::SizeBounds& available_size,
    const IconLabelBubbleView* host) const {
  return host->views::View::CalculatePreferredSize(available_size);
}

void StandardIconLabelBubbleAnimationLayoutStrategy::UpdateAnimationProgress(
    IconLabelBubbleView* host,
    double progress) {}

std::optional<base::TimeDelta>
StandardIconLabelBubbleAnimationLayoutStrategy::GetAnimationDuration(bool show) const {
  return std::nullopt;
}

void StandardIconLabelBubbleAnimationLayoutStrategy::SetupAnimation(
    gfx::SlideAnimation* animation,
    bool show) const {}

void StandardIconLabelBubbleAnimationLayoutStrategy::ResetAnimation(
    IconLabelBubbleView* host,
    bool show_label) {
  host->label()->SetVisible(show_label);
}

void StandardIconLabelBubbleAnimationLayoutStrategy::OnAnimationEnded(
    IconLabelBubbleView* host) {
  if (!host->is_animation_paused_) {
    // The label is shown at the start of animating in.
    // This ensures the label is hidden at the end of animating out.
    if (!host->IsAnimationShowing()) {
      host->label()->SetVisible(false);
    }

    // In some cases we want the text to disappear even after animating.
    // Subclasses override `ShouldShowLabelAfterAnimation` for custom behavior.
    // Default behavior is when we do not show separator, the label should
    // collapse.
    host->ResetSlideAnimation(host->ShouldShowLabelAfterAnimation());
    host->PreferredSizeChanged();
  }
  views::InkDrop::Get(host)->GetInkDrop()->SetShowHighlightOnHover(true);
  views::InkDrop::Get(host)->GetInkDrop()->SetShowHighlightOnFocus(
      !views::FocusRing::Get(host));
  host->UpdateBackground();
  host->UpdateBorder();
}
