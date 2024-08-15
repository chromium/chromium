// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/tabs/tab_style.h"

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

class ChromeRefresh2023TabStyle : public TabStyle {
 public:
  ~ChromeRefresh2023TabStyle() override = default;
  int GetStandardWidth() const override;
  int GetPinnedWidth() const override;
  int GetMinimumActiveWidth() const override;
  int GetMinimumInactiveWidth() const override;
  int GetTopCornerRadius() const override;
  int GetBottomCornerRadius() const override;
  int GetTabOverlap() const override;
  gfx::Size GetPreviewImageSize() const override;
  gfx::Size GetSeparatorSize() const override;
  gfx::Insets GetSeparatorMargins() const override;
  int GetSeparatorCornerRadius() const override;
  int GetDragHandleExtension(int height) const override;
  SkColor GetTabBackgroundColor(
      TabSelectionState state,
      bool hovered,
      bool frame_active,
      const ui::ColorProvider& color_provider) const override;
  gfx::Insets GetContentsInsets() const override;
  float GetSelectedTabOpacity() const override;
};

}  // namespace

TabStyle::~TabStyle() = default;

int ChromeRefresh2023TabStyle::GetStandardWidth() const {
  // The standard tab width is 240 DIP including both separators.
  constexpr int kTabWidth = 240;
  // The overlap includes one separator, so subtract it here.
  return kTabWidth + GetTabOverlap() - GetSeparatorSize().width();
}

int ChromeRefresh2023TabStyle::GetPinnedWidth() const {
  constexpr int kTabPinnedContentWidth = 24;
  return kTabPinnedContentWidth + GetContentsInsets().left() +
         GetContentsInsets().right();
}

int ChromeRefresh2023TabStyle::GetMinimumActiveWidth() const {
  const int close_button_size = GetLayoutConstant(TAB_CLOSE_BUTTON_SIZE);
  const gfx::Insets insets = GetContentsInsets();
  const int min_active_width =
      close_button_size + insets.left() + insets.right();
  if (base::FeatureList::IsEnabled(tabs::kScrollableTabStrip)) {
    return std::max(
        min_active_width,
        base::GetFieldTrialParamByFeatureAsInt(
            tabs::kScrollableTabStrip,
            tabs::kMinimumTabWidthFeatureParameterName, min_active_width));
  }
  return min_active_width;
}

int ChromeRefresh2023TabStyle::GetMinimumInactiveWidth() const {
  // Allow tabs to shrink until they appear to be 16 DIP wide excluding
  // outer corners.
  constexpr int kInteriorWidth = 16;
  // The overlap contains the trailing separator that is part of the interior
  // width; avoid double-counting it.
  int min_inactive_width =
      kInteriorWidth - GetSeparatorSize().width() + GetTabOverlap();

  if (base::FeatureList::IsEnabled(tabs::kScrollableTabStrip)) {
    return std::max(
        min_inactive_width,
        base::GetFieldTrialParamByFeatureAsInt(
            tabs::kScrollableTabStrip,
            tabs::kMinimumTabWidthFeatureParameterName, min_inactive_width));
  }

  return min_inactive_width;
}

int ChromeRefresh2023TabStyle::GetTopCornerRadius() const {
  return 10;
}

int ChromeRefresh2023TabStyle::GetBottomCornerRadius() const {
  return 12;
}

int ChromeRefresh2023TabStyle::GetTabOverlap() const {
  // The overlap removes the width and the margins of the separator.
  const float total_separator_width = GetSeparatorMargins().left() +
                                      GetSeparatorSize().width() +
                                      GetSeparatorMargins().right();
  return 2 * GetBottomCornerRadius() - total_separator_width;
}

gfx::Size ChromeRefresh2023TabStyle::GetPreviewImageSize() const {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetStandardWidth();
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

gfx::Size ChromeRefresh2023TabStyle::GetSeparatorSize() const {
  return gfx::Size(kChromeRefreshSeparatorThickness,
                   kChromeRefreshSeparatorHeight);
}

gfx::Insets ChromeRefresh2023TabStyle::GetSeparatorMargins() const {
  return gfx::Insets::TLBR(GetLayoutConstant(TAB_STRIP_PADDING),
                           kChromeRefreshSeparatorHorizontalMargin,
                           GetLayoutConstant(TAB_STRIP_PADDING),
                           kChromeRefreshSeparatorHorizontalMargin);
}

int ChromeRefresh2023TabStyle::GetSeparatorCornerRadius() const {
  return GetSeparatorSize().width() / 2;
}

int ChromeRefresh2023TabStyle::GetDragHandleExtension(int height) const {
  return 6;
}

SkColor ChromeRefresh2023TabStyle::GetTabBackgroundColor(
    const TabSelectionState state,
    const bool hovered,
    const bool frame_active,
    const ui::ColorProvider& color_provider) const {
  switch (state) {
    case TabStyle::TabSelectionState::kActive: {
      constexpr ui::ColorId kActiveColorIds[2] = {
          kColorTabBackgroundActiveFrameInactive,
          kColorTabBackgroundActiveFrameActive};
      return color_provider.GetColor(kActiveColorIds[frame_active]);
    }
    case TabStyle::TabSelectionState::kSelected: {
      constexpr ui::ColorId kSelectedColorIds[2][2] = {
          {kColorTabBackgroundSelectedFrameInactive,
           kColorTabBackgroundSelectedFrameActive},
          {kColorTabBackgroundSelectedHoverFrameInactive,
           kColorTabBackgroundSelectedHoverFrameActive}};
      return color_provider.GetColor(kSelectedColorIds[hovered][frame_active]);
    }
    case TabStyle::TabSelectionState::kInactive: {
      constexpr ui::ColorId kInactiveColorIds[2][2] = {
          {kColorTabBackgroundInactiveFrameInactive,
           kColorTabBackgroundInactiveFrameActive},
          {kColorTabBackgroundInactiveHoverFrameInactive,
           kColorTabBackgroundInactiveHoverFrameActive}};
      return color_provider.GetColor(kInactiveColorIds[hovered][frame_active]);
    }
    default:
      NOTREACHED();
  }
}

gfx::Insets ChromeRefresh2023TabStyle::GetContentsInsets() const {
  return gfx::Insets::TLBR(
      kChromeRefreshTabVerticalPadding + GetLayoutConstant(TAB_STRIP_PADDING),
      GetBottomCornerRadius() + kChromeRefreshTabHorizontalPadding,
      kChromeRefreshTabVerticalPadding + GetLayoutConstant(TAB_STRIP_PADDING),
      GetBottomCornerRadius() + kChromeRefreshTabHorizontalPadding);
}

float ChromeRefresh2023TabStyle::GetSelectedTabOpacity() const {
  return kDefaultSelectedTabOpacity;
}

// static
const TabStyle* TabStyle::Get() {
  static TabStyle* const tab_style =
      static_cast<TabStyle*>(new ChromeRefresh2023TabStyle());

  return tab_style;
}
