// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fullscreen_control/color_mixer.h"

#include "components/color/color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace fullscreen {

void AddColorMixer(ui::ColorProvider* provider,
                   const ui::ColorProviderKey& key) {
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[color::kFullscreenNotificationOpaqueBackgroundColor] = {
      SkColorSetRGB(0x28, 0x2c, 0x32)};
  mixer[color::kFullscreenNotificationTransparentBackgroundColor] = {
      ui::SetAlpha(color::kFullscreenNotificationOpaqueBackgroundColor, 0xcc)};
}

}  // namespace fullscreen
