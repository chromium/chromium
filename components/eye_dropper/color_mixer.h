// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EYE_DROPPER_COLOR_MIXER_H_
#define COMPONENTS_EYE_DROPPER_COLOR_MIXER_H_

namespace ui {
class ColorProvider;
struct ColorProviderKey;
}  // namespace ui

namespace eye_dropper {

// Adds a color mixer to `provider` that provides EyeDropper colors.
void AddColorMixer(ui::ColorProvider* provider,
                   const ui::ColorProviderKey& key);

}  // namespace eye_dropper

#endif  // COMPONENTS_EYE_DROPPER_COLOR_MIXER_H_
