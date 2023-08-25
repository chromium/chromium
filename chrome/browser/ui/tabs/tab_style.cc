// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_style.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/views/layout/layout_provider.h"

namespace {

// Thickness in DIPs of the separator painted on the left and right edges of
// the tab.
constexpr int kGM2SeparatorThickness = 1;
constexpr int kChromeRefreshSeparatorThickness = 2;
constexpr int kChromeRefreshSeparatorHorizontalMargin = 2;
// TODO (crbug.com/1451400): This constant should be in LayoutConstants.
constexpr int kChromeRefreshSeparatorHeight = 16;

// The padding from the top of the tab to the content area.
constexpr int kChromeRefreshTabVerticalPadding = 6;
constexpr int kChromeRefreshTabHorizontalPadding = 8;

class GM2TabStyle : public TabStyle {
 public:
  ~GM2TabStyle() override = default;
  int GetStandardWidth() const override;
  int GetPinnedWidth() const override;
  int GetMinimumActiveWidth() const override;
  int GetMinimumInactiveWidth() const override;
  int GetTabOverlap() const override;
  gfx::Size GetSeparatorSize() const override;
  gfx::Insets GetSeparatorMargins() const override;
  int GetSeparatorCornerRadius() const override;
  int GetDragHandleExtension(int height) const override;
  gfx::Size GetPreviewImageSize() const override;
  int GetTopCornerRadius() const override;
  int GetBottomCornerRadius() const override;
  SkColor GetTabBackgroundColor(
      TabSelectionState state,
      bool hovered,
      bool frame_active,
      const ui::ColorProvider& color_provider) const override;
  float GetSelectedTabOpacity() const override;
  gfx::Insets GetContentsInsets() const override;
};
class ChromeRefresh2023TabStyle : public GM2TabStyle {
 public:
  ~ChromeRefresh2023TabStyle() override = default;
  int GetTopCornerRadius() const override;
  int GetBottomCornerRadius() const override;
  int GetTabOverlap() const override;
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
};

}  // namespace

TabStyle::~TabStyle() = default;

int GM2TabStyle::GetStandardWidth() const {
  // The standard tab width is 240 DIP including both separators.
  constexpr int kTabWidth = 240;
  // The overlap includes one separator, so subtract it here.
  return kTabWidth + GetTabOverlap() - GetSeparatorSize().width();
}

int GM2TabStyle::GetPinnedWidth() const {
  constexpr int kTabPinnedContentWidth = 24;
  return kTabPinnedContentWidth + GetContentsInsets().left() +
         GetContentsInsets().right();
}

int GM2TabStyle::GetMinimumActiveWidth() const {
  const int close_button_size = GetLayoutConstant(TAB_CLOSE_BUTTON_SIZE);
  const gfx::Insets insets = GetContentsInsets();
  const int min_active_width =
      close_button_size + insets.left() + insets.right();
  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    return std::max(
        min_active_width,
        base::GetFieldTrialParamByFeatureAsInt(
            features::kScrollableTabStrip,
            features::kMinimumTabWidthFeatureParameterName, min_active_width));
  }
  return min_active_width;
}

int GM2TabStyle::GetMinimumInactiveWidth() const {
  // Allow tabs to shrink until they appear to be 16 DIP wide excluding
  // outer corners.
  constexpr int kInteriorWidth = 16;
  // The overlap contains the trailing separator that is part of the interior
  // width; avoid double-counting it.
  int min_inactive_width =
      kInteriorWidth - GetSeparatorSize().width() + GetTabOverlap();

  if (base::FeatureList::IsEnabled(features::kScrollableTabStrip)) {
    return std::max(min_inactive_width,
                    base::GetFieldTrialParamByFeatureAsInt(
                        features::kScrollableTabStrip,
                        features::kMinimumTabWidthFeatureParameterName,
                        min_inactive_width));
  }

  return min_inactive_width;
}

int GM2TabStyle::GetTabOverlap() const {
  return GetBottomCornerRadius() * 2 + GetSeparatorSize().width();
}

int GM2TabStyle::GetDragHandleExtension(int height) const {
  return (height - GetSeparatorSize().height()) / 2 - 1;
}

gfx::Size GM2TabStyle::GetSeparatorSize() const {
  return gfx::Size(kGM2SeparatorThickness,
                   GetLayoutConstant(TAB_SEPARATOR_HEIGHT));
}

gfx::Insets GM2TabStyle::GetSeparatorMargins() const {
  // the separator is rendered inside of the tab content.
  return gfx::Insets::TLBR(0, GetSeparatorSize().width() * -1, 0,
                           GetSeparatorSize().width() * -1);
}

int GM2TabStyle::GetSeparatorCornerRadius() const {
  return 0;
}

gfx::Size GM2TabStyle::GetPreviewImageSize() const {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetStandardWidth();
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

int GM2TabStyle::GetTopCornerRadius() const {
  return views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
}

int GM2TabStyle::GetBottomCornerRadius() const {
  return views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
}

gfx::Insets GM2TabStyle::GetContentsInsets() const {
  return gfx::Insets::TLBR(0, GetBottomCornerRadius() * 2, 0,
                           GetBottomCornerRadius() * 2);
}

SkColor GM2TabStyle::GetTabBackgroundColor(
    const TabSelectionState state,
    bool hovered,
    const bool frame_active,
    const ui::ColorProvider& color_provider) const {
  const SkColor active_color = color_provider.GetColor(
      frame_active ? kColorTabBackgroundActiveFrameActive
                   : kColorTabBackgroundActiveFrameInactive);
  const SkColor inactive_color = color_provider.GetColor(
      frame_active ? kColorTabBackgroundInactiveFrameActive
                   : kColorTabBackgroundInactiveFrameInactive);

  if (hovered) {
    return active_color;
  }

  switch (state) {
    case TabStyle::TabSelectionState::kActive:
      return active_color;
    case TabStyle::TabSelectionState::kSelected:
      // TODO(tbergquist): This maybe should be done in a color mixer, with tab
      // selected states having their own color ids even in GM2.
      return color_utils::AlphaBlend(active_color, inactive_color,
                                     GetSelectedTabOpacity());
    case TabStyle::TabSelectionState::kInactive:
      return inactive_color;
    default:
      NOTREACHED_NORETURN();
  }
}

float GM2TabStyle::GetSelectedTabOpacity() const {
  return kDefaultSelectedTabOpacity;
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

gfx::Size ChromeRefresh2023TabStyle::GetSeparatorSize() const {
  return gfx::Size(kChromeRefreshSeparatorThickness,
                   kChromeRefreshSeparatorHeight);
}

gfx::Insets ChromeRefresh2023TabStyle::GetContentsInsets() const {
  return gfx::Insets::TLBR(
      kChromeRefreshTabVerticalPadding + GetLayoutConstant(TAB_STRIP_PADDING),
      GetBottomCornerRadius() + kChromeRefreshTabHorizontalPadding,
      kChromeRefreshTabVerticalPadding + GetLayoutConstant(TAB_STRIP_PADDING),
      GetBottomCornerRadius() + kChromeRefreshTabHorizontalPadding);
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
      NOTREACHED_NORETURN();
  }
}

// static
const TabStyle* TabStyle::Get() {
  static TabStyle* const tab_style =
      features::IsChromeRefresh2023()
          ? static_cast<TabStyle*>(new ChromeRefresh2023TabStyle())
          : static_cast<TabStyle*>(new GM2TabStyle());

  return tab_style;
}
