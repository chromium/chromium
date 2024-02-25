// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/eye_dropper/color_mixer.h"

#include "components/color/color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace eye_dropper {

void AddColorMixer(ui::ColorProvider* provider,
                   const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[color::kColorEyedropperBoundary] = {SK_ColorDKGRAY};
  mixer[color::kColorEyedropperCentralPixelInnerRing] = {SK_ColorBLACK};
  mixer[color::kColorEyedropperCentralPixelOuterRing] = {SK_ColorWHITE};
  mixer[color::kColorEyedropperGrid] = {SK_ColorGRAY};
}

}  // namespace eye_dropper
