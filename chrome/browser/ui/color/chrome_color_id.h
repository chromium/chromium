// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_

#include "ui/color/color_id.h"

// TODO(pkasting): Add the rest of the colors.

// clang-format off
#define CHROME_COLOR_IDS \
  /* Bookmark Bar output colors. */ \
  E(kColorBookmarkText, ThemeProperties::COLOR_BOOKMARK_TEXT, \
    kChromeColorsStart) \
  /* Download Shelf output colors. */ \
  E(kColorDownloadShelf, ThemeProperties::COLOR_DOWNLOAD_SHELF) \
  E(kColorDownloadShelfButtonBackground, \
    ThemeProperties::COLOR_DOWNLOAD_SHELF_BUTTON_BACKGROUND) \
  E(kColorDownloadShelfButtonText, \
    ThemeProperties::COLOR_DOWNLOAD_SHELF_BUTTON_TEXT) \
  /* Omnibox output colors. */ \
  E(kColorOmniboxBackground, ThemeProperties::COLOR_OMNIBOX_BACKGROUND) \
  E(kColorOmniboxBackgroundHovered, \
    ThemeProperties::COLOR_OMNIBOX_BACKGROUND_HOVERED) \
  E(kColorOmniboxBubbleOutline, \
    ThemeProperties::COLOR_OMNIBOX_BUBBLE_OUTLINE) \
  E(kColorOmniboxBubbleOutlineExperimentalKeywordMode, \
    ThemeProperties::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE) \
  E(kColorOmniboxKeywordSelected, \
    ThemeProperties::COLOR_OMNIBOX_SELECTED_KEYWORD) \
  E(kColorOmniboxResultsBackground, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG) \
  E(kColorOmniboxResultsBackgroundHovered, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_HOVERED) \
  E(kColorOmniboxResultsBackgroundSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_SELECTED) \
  E(kColorOmniboxResultsIcon, ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON) \
  E(kColorOmniboxResultsIconSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON_SELECTED) \
  E(kColorOmniboxResultsTextDimmed, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED) \
  E(kColorOmniboxResultsTextDimmedSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED) \
  E(kColorOmniboxResultsTextSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED) \
  E(kColorOmniboxResultsUrl, ThemeProperties::COLOR_OMNIBOX_RESULTS_URL) \
  E(kColorOmniboxResultsUrlSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_URL_SELECTED) \
  E(kColorOmniboxSecurityChipDangerous, \
    ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS) \
  E(kColorOmniboxSecurityChipDefault, \
    ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT) \
  E(kColorOmniboxSecurityChipSecure, \
    ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_SECURE) \
  E(kColorOmniboxText, ThemeProperties::COLOR_OMNIBOX_TEXT) \
  E(kColorOmniboxTextDimmed, ThemeProperties::COLOR_OMNIBOX_TEXT_DIMMED) \
  /* Tab output colors. */ \
  E(kColorTabForegroundActiveFrameActive, \
    ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE) \
  E(kColorTabForegroundActiveFrameInactive, \
    ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_INACTIVE) \
  /* Toolbar output colors. */ \
  E(kColorToolbar, ThemeProperties::COLOR_TOOLBAR) \
  E(kColorToolbarButtonIcon, ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON) \
  E(kColorToolbarContentAreaSeparator, \
    ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR) \
  E(kColorToolbarText, ThemeProperties::COLOR_TOOLBAR_TEXT)

#include "ui/color/color_id_macros.inc"

enum ChromeColorIds : ui::ColorId {
  kChromeColorsStart = ui::kUiColorsEnd,

  CHROME_COLOR_IDS

  kChromeColorsEnd,
};

#include "ui/color/color_id_macros.inc"

// clang-format on

static_assert(ui::ColorId{kChromeColorsEnd} <= ui::ColorId{ui::kUiColorsLast},
              "Embedder colors must not exceed allowed space");

enum ChromeColorSetIds : ui::ColorSetId {
  kColorSetCustomTheme = ui::kUiColorSetsEnd,

  kChromeColorSetsEnd,
};

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
