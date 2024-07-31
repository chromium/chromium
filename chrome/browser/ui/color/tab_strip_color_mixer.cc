// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/tab_strip_color_mixer.h"

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

void AddTabStripColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderKey& key) {
  using TP = ThemeProperties;
  struct ColorPropertiesMapEntry {
    int property_id;
    ChromeColorIds color_id;
  };
  static constexpr ColorPropertiesMapEntry kFgPropertiesMap[] = {
      {TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE,
       kColorTabForegroundActiveFrameActive},
      {TP::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE,
       kColorTabForegroundActiveFrameInactive},
      {TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE,
       kColorTabForegroundInactiveFrameActive},
      {TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_INACTIVE,
       kColorTabForegroundInactiveFrameInactive},
  };
  static constexpr ColorPropertiesMapEntry kBgPropertiesMap[]{
      {TP::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE,
       kColorTabBackgroundActiveFrameActive},
      {TP::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE,
       kColorTabBackgroundActiveFrameInactive},
      {TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE,
       kColorTabBackgroundInactiveFrameActive},
      {TP::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE,
       kColorTabBackgroundInactiveFrameInactive},
  };
  // These are the default tab fore/background color mappings.
  static constexpr const auto kBgColorMap =
      base::MakeFixedFlatMap<ChromeColorIds, int>(
          {{kColorTabBackgroundActiveFrameActive, kColorToolbar},
           {kColorTabBackgroundActiveFrameInactive,
            kColorTabBackgroundActiveFrameActive},
           {kColorTabBackgroundInactiveFrameActive, ui::kColorFrameActive},
           {kColorTabBackgroundInactiveFrameInactive,
            ui::kColorFrameInactive}});
  static constexpr const auto kFgColorMap =
      base::MakeFixedFlatMap<ChromeColorIds, ChromeColorIds>(
          {{kColorTabForegroundActiveFrameActive, kColorToolbarText},
           {kColorTabForegroundActiveFrameInactive,
            kColorTabForegroundActiveFrameActive},
           {kColorTabForegroundInactiveFrameActive, kColorToolbarText},
           {kColorTabForegroundInactiveFrameInactive,
            kColorTabForegroundInactiveFrameActive}});
  static constexpr const auto kTabFgToBgMap =
      base::MakeFixedFlatMap<ChromeColorIds, ChromeColorIds>(
          {{kColorTabForegroundActiveFrameActive,
            kColorTabBackgroundActiveFrameActive},
           {kColorTabForegroundActiveFrameInactive,
            kColorTabBackgroundActiveFrameInactive},
           {kColorTabForegroundInactiveFrameActive,
            kColorTabBackgroundInactiveFrameActive},
           {kColorTabForegroundInactiveFrameInactive,
            kColorTabBackgroundInactiveFrameInactive}});
  // These contrast ratios should match the actual ratios in the default theme
  // colors when no system colors are involved, except for the inactive tab/
  // inactive frame case, which has been raised from 4.48 to 4.5 to meet
  // accessibility guidelines.
  static constexpr const auto kTabFgToContrastMap =
      base::MakeFixedFlatMap<ChromeColorIds, float>(
          {{kColorTabForegroundActiveFrameActive, 10.46f},
           {kColorTabForegroundActiveFrameInactive, 5.0f},
           {kColorTabForegroundInactiveFrameActive, 7.98f},
           {kColorTabForegroundInactiveFrameInactive, 4.5f}});

  ui::ColorMixer& mixer = provider->AddMixer();

  SkColor color;
  for (auto& entry : kFgPropertiesMap) {
    ui::ColorTransform color_transform = {kFgColorMap.at(entry.color_id)};
    if (key.custom_theme &&
        key.custom_theme->GetColor(entry.property_id, &color)) {
      mixer[entry.color_id] = {color};
    } else {
      if (entry.color_id == kColorTabForegroundInactiveFrameInactive &&
          key.custom_theme &&
          key.custom_theme->GetColor(
              TP::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE, &color)) {
        color_transform = {color};
      }
      ui::ColorTransform background_color_transform = {
          kTabFgToBgMap.at(entry.color_id)};
      // If a custom theme sets a background tab text color for active but not
      // inactive windows, generate the inactive color by blending the active
      // one at 75% as we do in the default theme.
      if (entry.color_id == kColorTabForegroundInactiveFrameInactive ||
          entry.color_id == kColorTabForegroundActiveFrameInactive) {
        color_transform =
            ui::AlphaBlend(color_transform, background_color_transform,
                           0.75f * SK_AlphaOPAQUE);
      }
      // To minimize any readability cost of custom system frame colors, try
      // to make the text reach the same contrast ratio that it would in the
      // default theme.
      auto target_transform =
          ui::GetColorWithMaxContrast(background_color_transform);
      mixer[entry.color_id] = ui::BlendForMinContrast(
          color_transform, background_color_transform, target_transform,
          kTabFgToContrastMap.at(entry.color_id));
    }
  }
  for (auto& entry : kBgPropertiesMap) {
    if (key.custom_theme &&
        key.custom_theme->GetColor(entry.property_id, &color)) {
      mixer[entry.color_id] = {color};
    } else if (entry.color_id == kColorTabBackgroundInactiveFrameActive ||
               entry.color_id == kColorTabBackgroundInactiveFrameInactive) {
      mixer[entry.color_id] = {ui::HSLShift(
          kBgColorMap.at(entry.color_id),
          GetThemeTint(ThemeProperties::TINT_BACKGROUND_TAB, key))};
    } else {
      mixer[entry.color_id] = {kBgColorMap.at(entry.color_id)};
    }
  }

  // Fallbacks for ChromeRefresh2023, these colors dont exist in the
  // GM2TabStyleViews version of the tabstrip. They approximately replicate GM2
  // behavior. The main difference is that the tab hover color in GM2 depends on
  // the tab width - narrower tabs have more opacity. We must chooses a single
  // opacity, so we go with one towards the more opaque end of the GM2 range.
  mixer[kColorTabBackgroundInactiveHoverFrameActive] = {
      ui::AlphaBlend(kColorTabBackgroundActiveFrameActive,
                     kColorTabBackgroundInactiveFrameActive,
                     /* 40% opacity */ 0.4 * SK_AlphaOPAQUE)};
  mixer[kColorTabBackgroundInactiveHoverFrameInactive] = {
      ui::AlphaBlend(kColorTabBackgroundActiveFrameInactive,
                     kColorTabBackgroundInactiveFrameInactive,
                     /* 40% opacity */ 0.4 * SK_AlphaOPAQUE)};
  mixer[kColorTabBackgroundSelectedFrameActive] = {
      ui::AlphaBlend(kColorTabBackgroundActiveFrameActive,
                     kColorTabBackgroundInactiveFrameActive,
                     /* 75% opacity */ 0.75 * SK_AlphaOPAQUE)};
  mixer[kColorTabBackgroundSelectedFrameInactive] = {
      ui::AlphaBlend(kColorTabBackgroundActiveFrameInactive,
                     kColorTabBackgroundInactiveFrameInactive,
                     /* 75% opacity */ 0.75 * SK_AlphaOPAQUE)};
  mixer[kColorTabBackgroundSelectedHoverFrameActive] = {
      ui::AlphaBlend(kColorTabBackgroundActiveFrameActive,
                     kColorTabBackgroundInactiveFrameActive,
                     /* 85% opacity */ 0.85 * SK_AlphaOPAQUE)};
  mixer[kColorTabBackgroundSelectedHoverFrameInactive] = {
      ui::AlphaBlend(kColorTabBackgroundActiveFrameInactive,
                     kColorTabBackgroundInactiveFrameInactive,
                     /* 85% opacity */ 0.85 * SK_AlphaOPAQUE)};

  mixer[kColorTabDividerFrameActive] = {kColorToolbar};
  mixer[kColorTabDividerFrameInactive] = {kColorToolbar};

#if !BUILDFLAG(IS_ANDROID)
  mixer[kColorTabDiscardRingFrameActive] = ui::BlendForMinContrastWithSelf(
      kColorTabBackgroundInactiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabDiscardRingFrameInactive] = ui::BlendForMinContrastWithSelf(
      kColorTabBackgroundInactiveFrameInactive,
      color_utils::kMinimumVisibleContrastRatio);
#endif

  mixer[kColorNewTabButtonForegroundFrameActive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorNewTabButtonForegroundFrameInactive] = {
      kColorTabForegroundActiveFrameInactive};
  mixer[kColorNewTabButtonBackgroundFrameActive] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {
      kColorTabBackgroundInactiveFrameInactive};
  mixer[kColorNewTabButtonFocusRing] = ui::PickGoogleColorTwoBackgrounds(
      ui::kColorFocusableBorderFocused,
      ui::GetResultingPaintColor(kColorNewTabButtonBackgroundFrameActive,
                                 ui::kColorFrameActive),
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorNewTabButtonInkDropFrameActive] =
      ui::GetColorWithMaxContrast(kColorNewTabButtonBackgroundFrameActive);
  mixer[kColorNewTabButtonInkDropFrameInactive] =
      ui::GetColorWithMaxContrast(kColorNewTabButtonBackgroundFrameInactive);

  // TODO (crbug.com/1399942): consolidate the new tab button color ids once the
  // refresh flag is enabled by default.
  mixer[kColorNewTabButtonCRForegroundFrameActive] = {kColorToolbarButtonIcon};
  mixer[kColorNewTabButtonCRForegroundFrameInactive] = {
      kColorToolbarButtonIconInactive};
  mixer[kColorNewTabButtonCRBackgroundFrameActive] = {kColorToolbar};
  mixer[kColorNewTabButtonCRBackgroundFrameInactive] = {kColorToolbar};
  mixer[kColorTabSearchButtonCRForegroundFrameActive] = {
      kColorToolbarButtonIcon};
  mixer[kColorTabSearchButtonCRForegroundFrameInactive] = {
      kColorToolbarButtonIconInactive};
  mixer[kColorTabStripControlButtonInkDrop] = ui::SetAlpha(
      kColorNewTabButtonInkDropFrameActive, std::ceil(0.16f * 255.0f));
  mixer[kColorTabStripControlButtonInkDropRipple] = ui::SetAlpha(
      kColorNewTabButtonInkDropFrameActive, std::ceil(0.14f * 255.0f));
  /* WebUI Tab Strip colors. */
  // TODO(crbug.com/40678998): Update the tab strip color to respond
  // appopriately to activation changes.
  mixer[kColorWebUiTabStripBackground] = {ui::kColorFrameActive};
  mixer[kColorWebUiTabStripFocusOutline] = {ui::kColorFocusableBorderFocused};
  mixer[kColorWebUiTabStripIndicatorRecording] = {ui::kColorAlertHighSeverity};
  mixer[kColorWebUiTabStripIndicatorPip] = {kColorTabThrobber};
  mixer[kColorWebUiTabStripIndicatorCapturing] = {kColorTabThrobber};
  mixer[kColorWebUiTabStripScrollbarThumb] =
      ui::SetAlpha(ui::GetColorWithMaxContrast(ui::kColorFrameActive),
                   /* 70% opacity */ 0.7 * 255);
  mixer[kColorWebUiTabStripTabActiveTitleBackground] = {
      kColorThumbnailTabBackground};
  mixer[kColorWebUiTabStripTabActiveTitleContent] = {
      kColorThumbnailTabForeground};
  mixer[kColorWebUiTabStripTabBackground] = {kColorToolbar};
  mixer[kColorWebUiTabStripTabBlocked] = {ui::kColorButtonBackgroundProminent};
  mixer[kColorWebUiTabStripTabLoadingSpinning] = {kColorTabThrobber};
  mixer[kColorWebUiTabStripTabSeparator] =
      ui::SetAlpha(kColorTabForegroundActiveFrameActive,
                   /* 16% opacity */ 0.16 * 255);
  mixer[kColorWebUiTabStripTabText] = {kColorTabForegroundActiveFrameActive};
  mixer[kColorWebUiTabStripTabWaitingSpinning] = {kColorTabThrobberPreconnect};
}
