// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_

#include "chrome/browser/ui/color/chrome_color_id.h"

enum class OmniboxPartState {
  NORMAL,
  HOVERED,
  SELECTED,
};

constexpr float kOmniboxOpacityHovered = 0.10f;
constexpr float kOmniboxOpacitySelected = 0.16f;

inline ui::ColorId GetOmniboxBackgroundColorId(OmniboxPartState state) {
  constexpr ui::ColorId kIds[] = {kColorOmniboxResultsBackground,
                                  kColorOmniboxResultsBackgroundHovered,
                                  kColorOmniboxResultsBackgroundSelected};
  return kIds[static_cast<size_t>(state)];
}

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_
