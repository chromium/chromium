// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_

#include "ui/color/color_id.h"

// TODO(pkasting): Add the rest of the colors.

// clang-format off
#define COMMON_CHROME_COLOR_IDS \
  /* Bookmark Bar colors. */ \
  E(kColorBookmarkBarBackground, \
    ThemeProperties::COLOR_BOOKMARK_BAR_BACKGROUND, kChromeColorsStart) \
  E(kColorBookmarkBarForeground, ThemeProperties::COLOR_BOOKMARK_TEXT) \
  E(kColorBookmarkBarSeparator, ThemeProperties::COLOR_BOOKMARK_SEPARATOR) \
  E(kColorBookmarkButtonIcon, ThemeProperties::COLOR_BOOKMARK_BUTTON_ICON) \
  E(kColorBookmarkFavicon, ThemeProperties::COLOR_BOOKMARK_FAVICON) \
  E_CPONLY(kColorBookmarkFolderIcon) \
  /* Download Shelf colors. */ \
  E(kColorDownloadShelf, ThemeProperties::COLOR_DOWNLOAD_SHELF) \
  E(kColorDownloadShelfButtonBackground, \
    ThemeProperties::COLOR_DOWNLOAD_SHELF_BUTTON_BACKGROUND) \
  E(kColorDownloadShelfButtonText, \
    ThemeProperties::COLOR_DOWNLOAD_SHELF_BUTTON_TEXT) \
  E_CPONLY(kColorDownloadToolbarButtonActive) \
  E_CPONLY(kColorDownloadToolbarButtonInactive) \
  E_CPONLY(kColorDownloadToolbarButtonRingBackground) \
  /* Google branding colors. */ \
  E_CPONLY(kColorGooglePayLogo) \
  /* Location bar colors. */ \
  E(kColorLocationBarBorder, ThemeProperties::COLOR_LOCATION_BAR_BORDER) \
  /* New Tab Page colors. */ \
  E(kColorNewTabPageBackground, ThemeProperties::COLOR_NTP_BACKGROUND) \
  E(kColorNewTabPageHeader, ThemeProperties::COLOR_NTP_HEADER) \
  E(kColorNewTabPageText, ThemeProperties::COLOR_NTP_TEXT) \
  /* Omnibox colors. */ \
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
  /* Tab colors. */ \
  E(kColorTabBackgroundActiveFrameActive, \
    ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE) \
  E(kColorTabBackgroundActiveFrameInactive, \
    ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE) \
  E(kColorTabBackgroundInactiveFrameActive, \
    ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE) \
  E(kColorTabBackgroundInactiveFrameInactive, \
    ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE) \
  E_CPONLY(kColorTabForegroundActiveFrameActive) \
  E_CPONLY(kColorTabForegroundActiveFrameInactive) \
  E_CPONLY(kColorTabForegroundInactiveFrameActive) \
  E_CPONLY(kColorTabForegroundInactiveFrameInactive) \
  E(kColorTabGroupContextMenuBlue, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_BLUE) \
  E(kColorTabGroupContextMenuCyan, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_CYAN) \
  E(kColorTabGroupContextMenuGreen, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_GREEN) \
  E(kColorTabGroupContextMenuGrey, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_GREY) \
  E(kColorTabGroupContextMenuPink, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_PINK) \
  E(kColorTabGroupContextMenuPurple, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_PURPLE) \
  E(kColorTabGroupContextMenuOrange, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_ORANGE) \
  E(kColorTabGroupContextMenuRed, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_RED) \
  E(kColorTabGroupContextMenuYellow, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_YELLOW) \
  /* Toolbar colors. */ \
  E(kColorToolbar, ThemeProperties::COLOR_TOOLBAR) \
  E(kColorToolbarButtonIcon, ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON) \
  E(kColorToolbarContentAreaSeparator, \
    ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR) \
  E(kColorToolbarSeparator, \
    ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR) \
  E(kColorToolbarText, ThemeProperties::COLOR_TOOLBAR_TEXT)

#if BUILDFLAG(IS_WIN)
#define CHROME_NATIVE_COLOR_IDS \
    /* The colors of the 1px border around the window on Windows 10. */ \
    E(kColorAccentBorderActive, ThemeProperties::COLOR_ACCENT_BORDER_ACTIVE) \
    E(kColorAccentBorderInactive, ThemeProperties::COLOR_ACCENT_BORDER_INACTIVE)
#else
#define CHROME_NATIVE_COLOR_IDS
#endif  // BUILDFLAG(IS_WIN)

#define CHROME_COLOR_IDS COMMON_CHROME_COLOR_IDS CHROME_NATIVE_COLOR_IDS

#include "ui/color/color_id_macros.inc"

enum ChromeColorIds : ui::ColorId {
  kChromeColorsStart = ui::kUiColorsEnd,

  CHROME_COLOR_IDS

  kChromeColorsEnd,
};

#include "ui/color/color_id_macros.inc"

// clang-format on

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
