// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_style.h"

#include <array>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/layout/layout_provider.h"

namespace {

// Thickness in DIPs of the separator painted on the left and right edges of
// the tab.
constexpr int kChromeRefreshSeparatorThickness = 2;
constexpr int kChromeRefreshSeparatorHorizontalMargin = 2;
// TODO (crbug.com/1451400): This constant should be in LayoutConstants.
constexpr int kChromeRefreshSeparatorHeight = 16;

// The padding from the top of the tab to the content area.
constexpr int kChromeRefreshTabVerticalPadding = 6;
constexpr int kChromeRefreshTabHorizontalPadding = 8;

// The standard tab width is 232 DIP, excluding separators and overlap.
constexpr int kTabWidth = 232;

}  // namespace

TabStyle::~TabStyle() = default;

int TabStyle::GetStandardHeight() const {
  return GetLayoutConstant(TAB_STRIP_HEIGHT);
}

int TabStyle::GetStandardWidth(const bool is_split) const {
  if (is_split) {
    // Split tabs appear as half width with one bottom extension. They also must
    // include half the tab overlap as the tabs fill the space between them.
    return kTabWidth / 2 + GetBottomCornerRadius() + GetTabOverlap() / 2;
  } else {
    // The full width includes two extensions with the bottom corner radius.
    return kTabWidth + 2 * GetBottomCornerRadius();
  }
}

int TabStyle::GetPinnedWidth(const bool is_split) const {
  constexpr int kTabPinnedContentWidth = 24;
  const int standard_pinned_width = kTabPinnedContentWidth +
                                    GetContentsInsets().left() +
                                    GetContentsInsets().right();
  if (is_split) {
    // Split tabs will recoup half of the tab overlap to reduce extra
    // whitespace.
    return standard_pinned_width - GetTabOverlap() / 2;
  }
  return standard_pinned_width;
}

int TabStyle::GetMinimumActiveWidth(const bool is_split) const {
  const int close_button_size = GetLayoutConstant(TAB_CLOSE_BUTTON_SIZE);
  const gfx::Insets insets = GetContentsInsets();
  const int min_active_width =
      close_button_size + insets.left() + insets.right();

  if (is_split) {
    // Only have one set of horizontal padding between tabs in an active split.
    return min_active_width - kChromeRefreshTabHorizontalPadding / 2;
  }

  return min_active_width;
}

int TabStyle::GetMinimumInactiveWidth() const {
  // Allow tabs to shrink until they appear to be 16 DIP wide excluding
  // outer corners.
  constexpr int kInteriorWidth = 16;
  // The overlap contains the trailing separator that is part of the interior
  // width; avoid double-counting it.
  int min_inactive_width =
      kInteriorWidth - GetSeparatorSize().width() + GetTabOverlap();

  return min_inactive_width;
}

int TabStyle::GetTopCornerRadius() const {
  return 10;
}

int TabStyle::GetBottomCornerRadius() const {
  return 12;
}

int TabStyle::GetTabOverlap() const {
  // The overlap removes the width and the margins of the separator.
  const float total_separator_width =
      GetSeparatorMargins().width() + GetSeparatorSize().width();
  return 2 * GetBottomCornerRadius() - total_separator_width;
}

gfx::Size TabStyle::GetPreviewImageSize() const {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetStandardWidth(/*is_split*/ false);
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

gfx::Size TabStyle::GetSeparatorSize() const {
  return gfx::Size(kChromeRefreshSeparatorThickness,
                   kChromeRefreshSeparatorHeight);
}

gfx::Insets TabStyle::GetSeparatorMargins() const {
  return gfx::Insets::TLBR(GetLayoutConstant(TAB_STRIP_PADDING),
                           kChromeRefreshSeparatorHorizontalMargin,
                           GetLayoutConstant(TAB_STRIP_PADDING),
                           kChromeRefreshSeparatorHorizontalMargin);
}

int TabStyle::GetSeparatorCornerRadius() const {
  return GetSeparatorSize().width() / 2;
}

int TabStyle::GetDragHandleExtension(int height) const {
  return 6;
}

SkColor TabStyle::GetTabBackgroundColor(
    const TabSelectionState state,
    const bool hovered,
    const bool frame_active,
    const ui::ColorProvider& color_provider) const {
  switch (state) {
    case TabStyle::TabSelectionState::kActive: {
      constexpr std::array<ui::ColorId, 2> kActiveColorIds = {
          kColorTabBackgroundActiveFrameInactive,
          kColorTabBackgroundActiveFrameActive};
      return color_provider.GetColor(kActiveColorIds[frame_active]);
    }
    case TabStyle::TabSelectionState::kSelected: {
      constexpr std::array<std::array<ui::ColorId, 2>, 2> kSelectedColorIds = {
          {{kColorTabBackgroundSelectedFrameInactive,
            kColorTabBackgroundSelectedFrameActive},
           {kColorTabBackgroundSelectedHoverFrameInactive,
            kColorTabBackgroundSelectedHoverFrameActive}}};
      return color_provider.GetColor(kSelectedColorIds[hovered][frame_active]);
    }
    case TabStyle::TabSelectionState::kInactive: {
      constexpr std::array<std::array<ui::ColorId, 2>, 2> kInactiveColorIds = {
          {{kColorTabBackgroundInactiveFrameInactive,
            kColorTabBackgroundInactiveFrameActive},
           {kColorTabBackgroundInactiveHoverFrameInactive,
            kColorTabBackgroundInactiveHoverFrameActive}}};
      return color_provider.GetColor(kInactiveColorIds[hovered][frame_active]);
    }
    default:
      NOTREACHED();
  }
}

gfx::Insets TabStyle::GetContentsInsets() const {
  return gfx::Insets::TLBR(
      kChromeRefreshTabVerticalPadding + GetLayoutConstant(TAB_STRIP_PADDING),
      GetBottomCornerRadius() + kChromeRefreshTabHorizontalPadding,
      kChromeRefreshTabVerticalPadding + GetLayoutConstant(TAB_STRIP_PADDING),
      GetBottomCornerRadius() + kChromeRefreshTabHorizontalPadding);
}

float TabStyle::GetSelectedTabOpacity() const {
  return kDefaultSelectedTabOpacity;
}

// static
const TabStyle* TabStyle::Get() {
  static TabStyle* const tab_style = static_cast<TabStyle*>(new TabStyle());

  return tab_style;
}
