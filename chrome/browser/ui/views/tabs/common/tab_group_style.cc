// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"

#include <optional>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"

namespace {

constexpr int kHeaderChipVerticalInset = 2;
constexpr int kEmptyChipSize = 20;
constexpr int kFocusedChipHeight = 28;
constexpr int kCornerRadius = 6;
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
