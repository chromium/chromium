// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/color/color_mixers.h"

#include "build/build_config.h"
#include "components/eye_dropper/color_mixer.h"

namespace color {

void AddComponentsColorMixers(ui::ColorProvider* provider,
                              const ui::ColorProviderKey& key) {
#if defined(USE_AURA)
  eye_dropper::AddColorMixer(provider, key);
#endif
}

}  // namespace color
