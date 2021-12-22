// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixers.h"

#include "chrome/browser/ui/color/chrome_color_mixer.h"
#include "chrome/browser/ui/color/native_chrome_color_mixer.h"
#include "chrome/browser/ui/color/omnibox_color_mixer.h"

void AddChromeColorMixers(ui::ColorProvider* provider,
                          const ui::ColorProviderManager::Key& key) {
  AddChromeColorMixer(provider, key);
  AddNativeChromeColorMixer(provider, key);
  AddOmniboxColorMixer(provider, key);

  if (key.custom_theme) {
    key.custom_theme->AddColorMixers(provider, key);
  }
}
