// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_NATIVE_CHROME_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_NATIVE_CHROME_COLOR_MIXER_H_

#include "ui/color/color_provider_key.h"

namespace ui {
class ColorProvider;
}

// Adds a color mixer to |provider| that can override the default chrome colors.
// This function should be implemented on a per-platform basis in relevant
// subdirectories.
void AddNativeChromeColorMixer(ui::ColorProvider* provider,
                               const ui::ColorProviderKey& key);

#endif  // CHROME_BROWSER_UI_COLOR_NATIVE_CHROME_COLOR_MIXER_H_
