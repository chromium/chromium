// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_

#include "ui/color/color_id.h"

enum ChromeColorIds : ui::ColorId {
  // TODO(pkasting): Add the rest of the colors.

  // Omnibox output colors.
  kColorOmniboxBackground =
      ui::kUiColorsEnd,  // TODO(pkasting): Add a default/custom theme recipe
  kColorOmniboxBackgroundHovered,
  kColorOmniboxBubbleOutline,
  kColorOmniboxBubbleOutlineExperimentalKeywordMode,
  kColorOmniboxResultsBackground,
  kColorOmniboxResultsBackgroundHovered,
  kColorOmniboxResultsBackgroundSelected,
  kColorOmniboxResultsIcon,
  kColorOmniboxResultsIconSelected,
  kColorOmniboxResultsTextDimmed,
  kColorOmniboxResultsTextDimmedSelected,
  kColorOmniboxResultsTextSelected,
  kColorOmniboxResultsUrl,
  kColorOmniboxResultsUrlSelected,
  kColorOmniboxSecurityChipDangerous,
  kColorOmniboxSecurityChipDefault,
  kColorOmniboxSecurityChipSecure,
  kColorOmniboxSelectedKeyword,
  kColorOmniboxText,  // TODO(pkasting): Add a default/custom theme recipe
  kColorOmniboxTextDimmed,

  kColorToolbar,  // TODO(pkasting): Add a recipe

  kChromeColorsEnd,
};

static_assert(ui::ColorId{kChromeColorsEnd} <= ui::ColorId{ui::kUiColorsLast},
              "Embedder colors must not exceed allowed space");

enum ChromeColorSetIds : ui::ColorSetId {
  kColorSetCustomTheme = ui::kUiColorSetsEnd,

  kChromeColorSetsEnd,
};

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
