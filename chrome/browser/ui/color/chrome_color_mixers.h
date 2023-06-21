// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXERS_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXERS_H_

#include "ui/color/color_provider_key.h"

namespace ui {
class ColorProvider;
}

// Adds all chrome/-side color mixers to `provider`.
void AddChromeColorMixers(ui::ColorProvider* provider,
                          const ui::ColorProviderKey& key);

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXERS_H_
