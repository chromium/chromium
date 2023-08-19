// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_OMNIBOX_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_OMNIBOX_COLOR_MIXER_H_

#include "ui/color/color_provider_key.h"

namespace ui {
class ColorProvider;
}

// Adds a color mixer that contains recipes for omnibox colors to |provider|
// with |key|.
void AddOmniboxColorMixer(ui::ColorProvider* provider,
                          const ui::ColorProviderKey& key);

#endif  // CHROME_BROWSER_UI_COLOR_OMNIBOX_COLOR_MIXER_H_
