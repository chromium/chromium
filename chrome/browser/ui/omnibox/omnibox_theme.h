// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_

#include "chrome/browser/ui/color/chrome_color_id.h"

enum class OmniboxPartState { NORMAL, HOVERED, SELECTED, IPH };

constexpr float kOmniboxOpacityHovered = 0.10f;
constexpr float kOmniboxOpacitySelected = 0.16f;

inline ui::ColorId GetOmniboxBackgroundColorId(OmniboxPartState state) {
  // TODO(crbug.com/333762301): Update the background color for the IPH
  // suggestion.
  constexpr ui::ColorId kIds[] = {kColorOmniboxResultsBackground,
                                  kColorOmniboxResultsBackgroundHovered,
                                  kColorOmniboxResultsBackgroundSelected,
                                  kColorOmniboxResultsBackgroundIPH};
  return kIds[static_cast<size_t>(state)];
}

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_
