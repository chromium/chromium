// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXER_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"

namespace ui {
class ColorProvider;
}

// Adds a color mixer to |provider| that supplies default values for various
// chrome/ colors before taking into account any custom themes.
void AddChromeColorMixer(ui::ColorProvider* provider,
                         const ui::ColorProviderKey& key);

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXER_H_
