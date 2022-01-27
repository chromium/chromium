// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/autofill_color_mixer.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

void AddAutofillColorMixer(ui::ColorProvider* provider,
                           const ui::ColorProviderManager::Key& key) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ui::ColorMixer& mixer = provider->AddMixer();

  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  mixer[kColorGooglePayLogo] = {dark_mode ? SK_ColorWHITE
                                          : gfx::kGoogleGrey700};
#endif
}
