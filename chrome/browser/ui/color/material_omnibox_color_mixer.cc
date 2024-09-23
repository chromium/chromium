// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/material_omnibox_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"

void AddMaterialOmniboxColorMixer(ui::ColorProvider* provider,
                                  const ui::ColorProviderKey& key) {
  if (!ShouldApplyChromeMaterialOverrides(key)) {
    return;
  }

  // While both design systems continue to exist, the material recipes are
  // intended to leverage the existing chrome color mixers, overriding when
  // required to do so according to the new material spec.
  // TODO(crbug.com/40883435): Update color recipes to match UX mocks.
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[kColorLocationBarClearAllButtonIcon] = {ui::kColorSysOnSurfaceSubtle};
  mixer[kColorLocationBarClearAllButtonIconDisabled] =
      ui::GetResultingPaintColor(ui::kColorSysStateDisabled,
                                 kColorLocationBarClearAllButtonIcon);
  mixer[kColorToolbarBackgroundSubtleEmphasis] = {
      ui::kColorSysOmniboxContainer};
  mixer[kColorToolbarBackgroundSubtleEmphasisHovered] =
      ui::GetResultingPaintColor(ui::kColorSysStateHoverBrightBlendProtection,
                                 kColorToolbarBackgroundSubtleEmphasis);
}
