// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome/browser/ui/color/new_tab_page_color_mixer.h"
#include "components/search/ntp_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

ui::ColorTransform UseIfNonzeroAlpha(ui::ColorTransform transform) {
  const auto generator = [](ui::ColorTransform transform, SkColor input_color,
                            const ui::ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color =
        SkColorGetA(transform_color) ? transform_color : input_color;
    DVLOG(2) << "ColorTransform UseIfNonzeroAlpha:"
             << " Input Color: " << ui::SkColorName(input_color)
             << " Transform Color: " << ui::SkColorName(transform_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform));
}

ui::ColorTransform GetToolbarTopSeparatorColorTransform(
    ui::ColorTransform transform,
    bool high_contrast) {
  const auto generator = [](ui::ColorTransform transform, SkColor input_color,
                            const ui::ColorMixer& mixer) {
    const float kMinContrastRatio = 2.f;
    const SkColor toolbar = transform.Run(input_color, mixer);
    // Try a darker separator color first (even on dark themes).
    const SkColor separator =
        color_utils::BlendForMinContrast(toolbar, toolbar, gfx::kGoogleGrey900,
                                         kMinContrastRatio)
            .color;
    if (color_utils::GetContrastRatio(separator, toolbar) >= kMinContrastRatio)
      return separator;
    // If a darker separator didn't give good enough contrast, try a lighter
    // separator.
    return color_utils::BlendForMinContrast(toolbar, toolbar, SK_ColorWHITE,
                                            kMinContrastRatio)
        .color;
  };
  return high_contrast ? ui::GetColorWithMaxContrast(transform)
                       : base::BindRepeating(generator, std::move(transform));
}

}  // namespace

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  if (key.system_theme == ui::SystemTheme::kDefault)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorBookmarkBarSeparator] = {kColorToolbarSeparatorDefault};
  mixer[kColorBookmarkButtonIcon] = {kColorToolbarButtonIconDefault};
  mixer[kColorBookmarkFavicon] = {kColorToolbarButtonIcon};
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorInfoBarForeground] = {ui::kColorNativeLabelForeground};
  mixer[kColorInfoBarContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorLocationBarBorder] =
      UseIfNonzeroAlpha(ui::kColorNativeTextfieldBorderUnfocused);
  mixer[kColorNewTabButtonBackgroundFrameActive] = {SK_ColorTRANSPARENT};
  mixer[kColorNewTabButtonBackgroundFrameInactive] = {SK_ColorTRANSPARENT};
  mixer[kColorNewTabPageBackground] = {ui::kColorTextfieldBackground};
  mixer[kColorNewTabPageHeader] = {ui::kColorNativeButtonBorder};
  mixer[kColorNewTabPageLink] = {ui::kColorTextfieldSelectionBackground};
  mixer[kColorNewTabPageText] = {ui::kColorTextfieldForeground};
  mixer[kColorOmniboxText] = {ui::kColorTextfieldForeground};
  mixer[kColorToolbarBackgroundSubtleEmphasis] = {
      ui::kColorTextfieldBackground};
  mixer[kColorTabForegroundInactiveFrameActive] = {
      ui::kColorNativeTabForegroundInactiveFrameActive};
  mixer[kColorTabForegroundInactiveFrameInactive] = {
      ui::kColorNativeTabForegroundInactiveFrameInactive};
  mixer[kColorTabStrokeFrameActive] = {kColorToolbarTopSeparatorFrameActive};
  mixer[kColorTabStrokeFrameInactive] = {
      kColorToolbarTopSeparatorFrameInactive};
  mixer[kColorToolbar] = {ui::kColorNativeToolbarBackground};
  mixer[kColorToolbarButtonIcon] = {kColorToolbarText};
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarSeparator};
  mixer[kColorToolbarSeparator] = {ui::kColorNativeButtonBorder};
  mixer[kColorToolbarText] = {ui::kColorNativeLabelForeground};
  mixer[kColorToolbarTextDisabled] =
      ui::SetAlpha(kColorToolbarText, gfx::kDisabledControlAlpha);
  const bool high_contrast =
      key.contrast_mode == ui::ColorProviderKey::ContrastMode::kHigh;
  mixer[kColorToolbarTopSeparatorFrameActive] =
      GetToolbarTopSeparatorColorTransform(ui::kColorNativeToolbarBackground,
                                           high_contrast);
  mixer[kColorToolbarTopSeparatorFrameInactive] = {
      kColorToolbarTopSeparatorFrameActive};
}
