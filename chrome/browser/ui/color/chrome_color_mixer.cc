// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

void AddBaseColors(bool dark_mode, ui::ColorMixer& mixer) {
  if (dark_mode) {
    mixer[kColorOmniboxBackground] = {gfx::kGoogleGrey900};
    mixer[kColorOmniboxText] = {SK_ColorWHITE};
    mixer[kColorToolbar] = {SkColorSetRGB(0x35, 0x36, 0x3A)};
    mixer[kColorToolbarText] = {SK_ColorWHITE};
  } else {
    mixer[kColorOmniboxBackground] = {gfx::kGoogleGrey100};
    mixer[kColorOmniboxText] = {gfx::kGoogleGrey900};
    mixer[kColorToolbar] = {SK_ColorWHITE};
    mixer[kColorToolbarText] = {gfx::kGoogleGrey800};
  }
}

}  // namespace

void AddChromeColorMixer(ui::ColorProvider* provider,
                         const ui::ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  ui::ColorMixer& mixer = provider->AddMixer();

  AddBaseColors(dark_mode, mixer);

  mixer[kColorBookmarkText] = {kColorToolbarText};
  mixer[kColorDownloadShelf] = {kColorToolbar};
  mixer[kColorDownloadShelfButtonBackground] = {kColorDownloadShelf};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorAccent, kColorDownloadShelf,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorTabForegroundActiveFrameActive] = {kColorToolbarText};
  mixer[kColorTabForegroundActiveFrameInactive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorToolbarContentAreaSeparator] = {ui::kColorSeparator};

  if (dark_mode) {
    mixer[kColorTabGroupContextMenuGrey] = {gfx::kGoogleGrey300};
    mixer[kColorTabGroupContextMenuBlue] = {gfx::kGoogleBlue300};
    mixer[kColorTabGroupContextMenuRed] = {gfx::kGoogleRed300};
    mixer[kColorTabGroupContextMenuYellow] = {gfx::kGoogleYellow300};
    mixer[kColorTabGroupContextMenuGreen] = {gfx::kGoogleGreen300};
    mixer[kColorTabGroupContextMenuPink] = {gfx::kGooglePink300};
    mixer[kColorTabGroupContextMenuPurple] = {gfx::kGooglePurple300};
    mixer[kColorTabGroupContextMenuCyan] = {gfx::kGoogleCyan300};
    mixer[kColorTabGroupContextMenuOrange] = {gfx::kGoogleOrange300};
  } else {
    mixer[kColorTabGroupContextMenuGrey] = {gfx::kGoogleGrey700};
    mixer[kColorTabGroupContextMenuBlue] = {gfx::kGoogleBlue600};
    mixer[kColorTabGroupContextMenuRed] = {gfx::kGoogleRed600};
    mixer[kColorTabGroupContextMenuYellow] = {gfx::kGoogleYellow600};
    mixer[kColorTabGroupContextMenuGreen] = {gfx::kGoogleGreen700};
    mixer[kColorTabGroupContextMenuPink] = {gfx::kGooglePink700};
    mixer[kColorTabGroupContextMenuPurple] = {gfx::kGooglePurple500};
    mixer[kColorTabGroupContextMenuCyan] = {gfx::kGoogleCyan900};
    mixer[kColorTabGroupContextMenuOrange] = {gfx::kGoogleOrange400};
  }
}
