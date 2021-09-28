// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_OMNIBOX_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_OMNIBOX_COLOR_MIXER_H_

namespace ui {
class ColorProvider;
}

// Adds a color mixer to |provider| that contains recipes for omnibox colors,
// given whether they should be |high_contrast|.
// TODO(pkasting): Perhaps |high_contrast| should be a bit on the ColorProvider.
void AddOmniboxColorMixer(ui::ColorProvider* provider, bool high_contrast);

#endif  // CHROME_BROWSER_UI_COLOR_OMNIBOX_COLOR_MIXER_H_
