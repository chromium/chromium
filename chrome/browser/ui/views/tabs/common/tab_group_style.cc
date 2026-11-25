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

constexpr int kHeaderHeight = 20;
constexpr int kFocusedHeaderHeight = 28;
constexpr int kCornerRadius = 6;
constexpr int kFocusedCornerRadius = 10;
constexpr int kFocusedInnerCornerRadius = 2;
constexpr int kVerticalCornerRadius = 8;
constexpr int kVerticalHeaderHorizontalInset = 8;
constexpr int kTabGroupOverlapAdjustment = 2;
constexpr gfx::Insets kFocusedHeaderInsets = gfx::Insets::TLBR(6, 12, 6, 10);
constexpr gfx::Insets kFocusedUnfocusButtonInsets =
    gfx::Insets::TLBR(2, 10, 2, 10);

int GetHeaderCornerRadius(TabStripOrientation orientation,
                          bool in_focused_state) {
  if (orientation == TabStripOrientation::kVertical) {
    return kVerticalCornerRadius;
  }
  return in_focused_state ? kFocusedCornerRadius : kCornerRadius;
}

// Builds corner radii for one of the two adjacent elements in focus mode:
// the Unfocus button (`is_leading = true`) and the group header
// (`is_leading = false`). The inner corners face the adjacent element.
gfx::RoundedCornersF MakeAdjacentCornerRadii(TabStripOrientation orientation,
                                             float inner_radius,
                                             float outer_radius,
                                             bool is_leading) {
  if (orientation == TabStripOrientation::kVertical) {
    return is_leading ? gfx::RoundedCornersF(outer_radius, outer_radius,
                                             inner_radius, inner_radius)
                      : gfx::RoundedCornersF(inner_radius, inner_radius,
                                             outer_radius, outer_radius);
  }
  const bool inner_on_right = is_leading != base::i18n::IsRTL();
  return inner_on_right ? gfx::RoundedCornersF(outer_radius, inner_radius,
                                               inner_radius, outer_radius)
                        : gfx::RoundedCornersF(inner_radius, outer_radius,
                                               outer_radius, inner_radius);
}

float GetTrailingSeparatorOpacity(bool is_collapsed,
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

}  // namespace

// static
int TabGroupStyle::GetHeaderHeight(bool in_focused_state) {
  return in_focused_state ? kFocusedHeaderHeight : kHeaderHeight;
}

// static
gfx::Point TabGroupStyle::GetHeaderOffset(std::optional<int> header_height) {
  const int total_space =
      GetLayoutConstant(LayoutConstant::kTabStripHeight) -
      header_height.value_or(GetHeaderHeight(/*in_focused_state=*/false)) -
      GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap);
  return gfx::Point(GetLeadingGroupHeaderPadding(), total_space / 2);
}

// static
gfx::Insets TabGroupStyle::GetHeaderInsets(TabStripOrientation orientation,
                                           bool in_focused_state) {
  if (orientation == TabStripOrientation::kVertical) {
    return gfx::Insets::VH(0, kVerticalHeaderHorizontalInset);
  }
  return in_focused_state ? kFocusedHeaderInsets
                          : gfx::Insets::VH(0, kCornerRadius);
}

// static
gfx::RoundedCornersF TabGroupStyle::GetHeaderCornerRadii(
    TabStripOrientation orientation,
    float focus_progress) {
  const float resting_radius =
      GetHeaderCornerRadius(orientation, /*in_focused_state=*/false);
  const float inner_radius = gfx::Tween::FloatValueBetween(
      focus_progress, resting_radius, kFocusedInnerCornerRadius);
  const float outer_radius = gfx::Tween::FloatValueBetween(
      focus_progress, resting_radius,
      GetHeaderCornerRadius(orientation, /*in_focused_state=*/true));
  return MakeAdjacentCornerRadii(orientation, inner_radius, outer_radius,
                                 /*is_leading=*/false);
}

// static
int TabGroupStyle::GetUnfocusButtonHeight() {
  return kFocusedHeaderHeight;
}

// static
gfx::Insets TabGroupStyle::GetUnfocusButtonInsets(
    TabStripOrientation orientation) {
  return orientation == TabStripOrientation::kHorizontal
             ? kFocusedUnfocusButtonInsets
             : gfx::Insets::VH(0, kVerticalHeaderHorizontalInset);
}

// static
gfx::RoundedCornersF TabGroupStyle::GetUnfocusButtonCornerRadii(
    TabStripOrientation orientation,
    float focus_progress) {
  const float resting_radius =
      GetHeaderCornerRadius(orientation, /*in_focused_state=*/false);
  const float inner_radius = gfx::Tween::FloatValueBetween(
      focus_progress, resting_radius, kFocusedInnerCornerRadius);
  const float outer_radius =
      GetHeaderCornerRadius(orientation, /*in_focused_state=*/true);
  return MakeAdjacentCornerRadii(orientation, inner_radius, outer_radius,
                                 /*is_leading=*/true);
}

// static
int TabGroupStyle::GetOverlapAdjustment() {
  return kTabGroupOverlapAdjustment;
}

// static
int TabGroupStyle::GetPaddingBetweenCollapsedHeaders() {
  return TabStyle::Get()->GetTabOverlap() - 2 * GetOverlapAdjustment();
}

// static
int TabGroupStyle::GetLeadingGroupHeaderPadding() {
  return TabStyle::Get()->GetTabOverlap() - GetOverlapAdjustment();
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
  return TabStyle::Get()->GetTabOverlap() - GetOverlapAdjustment() +
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
