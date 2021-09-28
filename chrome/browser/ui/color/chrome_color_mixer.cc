// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixer.h"

#include "base/bind.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace {

constexpr float kMinOmniboxToolbarContrast = 1.3f;

ui::ColorTransform ChooseOmniboxBgBlendTarget() {
  return base::BindRepeating(
      [](SkColor input_color, const ui::ColorMixer& mixer) {
        const SkColor toolbar_color = mixer.GetResultColor(kColorToolbar);
        const SkColor endpoint_color =
            color_utils::GetEndpointColorWithMinContrast(toolbar_color);
        return (color_utils::GetContrastRatio(toolbar_color, endpoint_color) >=
                kMinOmniboxToolbarContrast)
                   ? endpoint_color
                   : color_utils::GetColorWithMaxContrast(endpoint_color);
      });
}

}  // namespace

void AddChromeColorMixer(ui::ColorProvider* provider) {
  ui::ColorMixer& mixer = provider->AddMixer();

  // TODO(pkasting): Pre-color pipeline this is only enabled for custom themes.
  // Agree on consistent behavior before enabling this.
  mixer[kColorOmniboxBackground] = ui::BlendForMinContrast(
      kColorToolbar, kColorToolbar, ChooseOmniboxBgBlendTarget(),
      kMinOmniboxToolbarContrast);
  mixer[kColorOmniboxText] =
      ui::GetColorWithMaxContrast(kColorOmniboxBackground);
  // TODO(tluk) Behavior change for dark mode to a darker toolbar color for
  // better color semantics. Follow up with UX team before landing change.
  mixer[kColorToolbar] = {ui::kColorPrimaryBackground};
}
