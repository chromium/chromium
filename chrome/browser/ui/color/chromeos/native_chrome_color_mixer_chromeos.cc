// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorBorealisSplashScreenBackground] = {
      SkColorSetRGB(0x35, 0x33, 0x32)};
  mixer[kColorBorealisSplashScreenForeground] = {
      SkColorSetRGB(0xD1, 0xD0, 0xCF)};
  mixer[kColorCaptionForeground] = {
      (key.color_mode == ui::ColorProviderKey::ColorMode::kLight)
          ? SkColorSetRGB(0x28, 0x28, 0x28)
          : SK_ColorWHITE};
  mixer[kColorSharesheetTargetButtonIconShadow] = {
      SkColorSetA(SK_ColorBLACK, 0x33)};
}
