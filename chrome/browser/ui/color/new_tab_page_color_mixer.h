// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_NEW_TAB_PAGE_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_NEW_TAB_PAGE_COLOR_MIXER_H_

#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_key.h"

namespace ui {
class ColorMixer;
class ColorProvider;
}  // namespace ui

// Adds a color mixer that contains recipes for NewTabPage colors to `provider`
// with `key`.
void AddNewTabPageColorMixer(ui::ColorProvider* provider,
                             const ui::ColorProviderKey& key);

// Allows the Linux mixer to override certain colors to be the light theme
// colors to retain the original color behavior in GTK+. See crbug.com/998903.
// This logic will be refactored once the NewTabPage comprehensive theming
// experiment has completed.
void AddWebThemeNewTabPageColors(ui::ColorMixer& mixer, bool dark_mode);

#endif  // CHROME_BROWSER_UI_COLOR_NEW_TAB_PAGE_COLOR_MIXER_H_
