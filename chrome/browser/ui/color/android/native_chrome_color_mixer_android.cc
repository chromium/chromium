// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/native_chrome_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"

void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // Native Android navigation draws the selected item with
  // on-secondary-container on a secondary-container background.
  mixer[kColorNavMenuItemSelected] = {ui::kColorSysOnSecondaryContainer};
  mixer[kColorNavMenuItemBackgroundSelected] = {
      ui::kColorSysSecondaryContainer};

  // History page colors. They map onto the Material roles that
  // ColorProviderBridgeImpl reads from the Android theme.
  const bool dark_mode =
      key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  mixer[kColorHistoryPageCardBackground] = {
      dark_mode ? ui::kColorSysSurface : ui::kColorSysSurfaceContainerLowest};
  mixer[kColorHistoryPageMenuBackground] = {
      dark_mode ? ui::kColorSysSurfaceContainerHigh : ui::kColorSysSurface};
  mixer[kColorHistoryPageSearchFieldBackground] = {
      dark_mode ? ui::kColorSysSurfaceContainerHigh
                : ui::kColorSysSurfaceContainerLowest};
  mixer[kColorHistoryPageSelectionToolbarBackground] = {
      dark_mode ? ui::kColorSysSurface : ui::kColorSysSurfaceContainerHigh};
}
