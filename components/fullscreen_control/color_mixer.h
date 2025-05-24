// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULLSCREEN_CONTROL_COLOR_MIXER_H_
#define COMPONENTS_FULLSCREEN_CONTROL_COLOR_MIXER_H_

namespace ui {
class ColorProvider;
struct ColorProviderKey;
}  // namespace ui

namespace fullscreen {

// Adds a color mixer to `provider` that provides Fullscreen Notification
// colors.
void AddColorMixer(ui::ColorProvider* provider,
                   const ui::ColorProviderKey& key);

}  // namespace fullscreen

#endif  // COMPONENTS_FULLSCREEN_CONTROL_COLOR_MIXER_H_
