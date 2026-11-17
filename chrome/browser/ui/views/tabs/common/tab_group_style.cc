// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"

#include <optional>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/common/horizontal_tab_style_views.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/focus_ring.h"

namespace {

constexpr int kHeaderChipVerticalInset = 2;
constexpr int kEmptyChipSize = 20;
constexpr int kFocusedChipHeight = 28;
constexpr int kCornerRadius = 6;
constexpr int kFocusedCornerRadius = 10;
constexpr int kFocusedInnerCornerRadius = 2;
constexpr int kVerticalCornerRadius = 8;
constexpr int kVerticalHeaderChipHorizontalInset = 8;
constexpr int kTabGroupOverlapAdjustment = 2;
constexpr gfx::Insets kFocusedHeaderChipInsets =
    gfx::Insets::TLBR(6, 12, 6, 10);
constexpr gfx::Insets kFocusedUnfocusChipInsets =
    gfx::Insets::TLBR(2, 10, 2, 10);

}  // namespace

// static
int TabGroupStyle::GetTabGroupOverlapAdjustment() {
  return kTabGroupOverlapAdjustment;
}

// static
int TabGroupStyle::GetChipCornerRadius(TabStripOrientation orientation) {
  return orientation == TabStripOrientation::kHorizontal
             ? kCornerRadius
             : kVerticalCornerRadius;
}

// static
int TabGroupStyle::GetFocusedChipCornerRadius(TabStripOrientation orientation) {
  return orientation == TabStripOrientation::kHorizontal
             ? kFocusedCornerRadius
             : kVerticalCornerRadius;
}

// static
gfx::RoundedCornersF TabGroupStyle::GetHeaderCornerRadii(
    TabStripOrientation orientation,
    bool in_focused_state,
    std::optional<float> animation_progress) {
  const float resting_radius = GetChipCornerRadius(orientation);
  if (!animation_progress.has_value() && !in_focused_state) {
    return gfx::RoundedCornersF(resting_radius);
  }
  const float progress = animation_progress.value_or(1.0f);
  const float inner_radius = gfx::Tween::FloatValueBetween(
      progress, resting_radius, kFocusedInnerCornerRadius);
  const float outer_radius = gfx::Tween::FloatValueBetween(
      progress, resting_radius, GetFocusedChipCornerRadius(orientation));
  if (orientation == TabStripOrientation::kVertical) {
    return gfx::RoundedCornersF(inner_radius, inner_radius, outer_radius,
                                outer_radius);
  }
  return base::i18n::IsRTL() ? gfx::RoundedCornersF(outer_radius, inner_radius,
                                                    inner_radius, outer_radius)
                             : gfx::RoundedCornersF(inner_radius, outer_radius,
                                                    outer_radius, inner_radius);
}

// static
gfx::RoundedCornersF TabGroupStyle::GetUnfocusButtonCornerRadii(
    TabStripOrientation orientation,
    std::optional<float> animation_progress) {
  const float resting_radius = GetChipCornerRadius(orientation);
  const float progress = animation_progress.value_or(1.0f);
  const float inner_radius = gfx::Tween::FloatValueBetween(
      progress, resting_radius, kFocusedInnerCornerRadius);
  const float outer_radius = GetFocusedChipCornerRadius(orientation);
  if (orientation == TabStripOrientation::kVertical) {
    return gfx::RoundedCornersF(outer_radius, outer_radius, inner_radius,
                                inner_radius);
  }
  return base::i18n::IsRTL() ? gfx::RoundedCornersF(inner_radius, outer_radius,
                                                    outer_radius, inner_radius)
                             : gfx::RoundedCornersF(outer_radius, inner_radius,
                                                    inner_radius, outer_radius);
}

// static
int TabGroupStyle::GetEmptyChipSize() {
  return kEmptyChipSize;
}

// static
int TabGroupStyle::GetFocusedChipHeight() {
  return kFocusedChipHeight;
}

// static
gfx::Point TabGroupStyle::GetTitleChipOffset(std::optional<int> chip_height) {
  const int total_space =
      GetLayoutConstant(LayoutConstant::kTabStripHeight) -
      chip_height.value_or(GetEmptyChipSize()) -
      GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap);
  return gfx::Point(GetLeadingGroupHeaderPadding(), total_space / 2);
}

// static
gfx::Insets TabGroupStyle::GetInsetsForHeaderChip(
    TabStripOrientation orientation) {
  return orientation == TabStripOrientation::kHorizontal
             ? gfx::Insets::VH(kHeaderChipVerticalInset, kCornerRadius)
             : gfx::Insets::VH(0, kVerticalHeaderChipHorizontalInset);
}

// static
gfx::Insets TabGroupStyle::GetInsetsForFocusedHeaderChip() {
  return kFocusedHeaderChipInsets;
}

// static
gfx::Insets TabGroupStyle::GetInsetsForFocusedUnfocusChip() {
  return kFocusedUnfocusChipInsets;
}

// static
int TabGroupStyle::GetPaddingBetweenCollapsedHeaders() {
  return TabStyle::Get()->GetTabOverlap() - 2 * GetTabGroupOverlapAdjustment();
}

// static
int TabGroupStyle::GetLeadingGroupHeaderPadding() {
  return TabStyle::Get()->GetTabOverlap() - GetTabGroupOverlapAdjustment();
}

// static
int TabGroupStyle::GetTrailingGroupHeaderPadding() {
  return GetLeadingGroupHeaderPadding();
}

// static
int TabGroupStyle::GetUnderlineStrokeThickness() {
  return views::FocusRing::kDefaultHaloThickness;
}

// static
int TabGroupStyle::GetUnderlineStrokeInset() {
  return TabStyle::Get()->GetTabOverlap() - GetTabGroupOverlapAdjustment() +
         GetUnderlineStrokeThickness();
}

// static
TabStyle::SeparatorOpacities TabGroupStyle::GetSeparatorOpacities(
    bool is_collapsed,
    bool is_dragging,
    int tab_count,
    const TabStyleViewDelegate* adjacent_tab) {
  const float trailing_opacity = GetTrailingSeparatorOpacity(
      is_collapsed, is_dragging, tab_count, adjacent_tab);

  // Return the opacities in physical order, rather than logical.
  if (base::i18n::IsRTL()) {
    return {trailing_opacity, 0.0f};
  }
  return {0.0f, trailing_opacity};
}

// static
float TabGroupStyle::GetTrailingSeparatorOpacity(
    bool is_collapsed,
    bool is_dragging,
    int tab_count,
    const TabStyleViewDelegate* adjacent_tab) {
  if (!is_collapsed || is_dragging) {
    return 0.0f;
  }

  // Do not show separators if the tab strip is in a decluttered state.
  if (features::IsTabStripDeclutterEnabled() &&
      tab_count >= TabStyle::kTabStripDeclutterMinTabsForSeparatorHide) {
    return 0.0f;
  }

  // If the following tab belongs to another group, that group's header sits
  // between this collapsed header and `adjacent_tab`.
  if (adjacent_tab && adjacent_tab->GetGroup().has_value()) {
    adjacent_tab = nullptr;
  }

  // Do not show when the adjacent tab is displaying a visible shape.
  if (adjacent_tab &&
      HorizontalTabStyleViews::HasVisibleBackground(adjacent_tab)) {
    return 0.0f;
  }

  // The header has no hover animation of its own, so only the adjacent tab's
  // hover state is taken into account.
  return HorizontalTabStyleViews::GetHoverInterpolatedSeparatorOpacity(
      /*for_layout=*/false, adjacent_tab, /*hover_value=*/0.0f);
}
