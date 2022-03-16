// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_

#include "ui/color/color_id.h"

// TODO(pkasting): Add the rest of the colors.

// clang-format off
#define COMMON_CHROME_COLOR_IDS \
  /* App menu colors. */ \
  E(kColorAppMenuHighlightSeverityLow, \
    ThemeProperties::COLOR_APP_MENU_HIGHLIGHT_SEVERITY_LOW, \
    kChromeColorsStart) \
  E(kColorAppMenuHighlightSeverityHigh, \
    ThemeProperties::COLOR_APP_MENU_HIGHLIGHT_SEVERITY_HIGH) \
  E(kColorAppMenuHighlightSeverityMedium, \
    ThemeProperties::COLOR_APP_MENU_HIGHLIGHT_SEVERITY_MEDIUM) \
  /* Avatar colors. */ \
  E(kColorAvatarButtonHighlightNormal, \
    ThemeProperties::COLOR_AVATAR_BUTTON_HIGHLIGHT_NORMAL) \
  E(kColorAvatarButtonHighlightSyncError, \
    ThemeProperties::COLOR_AVATAR_BUTTON_HIGHLIGHT_SYNC_ERROR) \
  E(kColorAvatarButtonHighlightSyncPaused, \
    ThemeProperties::COLOR_AVATAR_BUTTON_HIGHLIGHT_SYNC_PAUSED) \
  E_CPONLY(kColorAvatarStrokeLight) \
  /* Bookmark bar colors. */ \
  E(kColorBookmarkBarBackground, \
    ThemeProperties::COLOR_BOOKMARK_BAR_BACKGROUND) \
  E(kColorBookmarkBarForeground, ThemeProperties::COLOR_BOOKMARK_TEXT) \
  E(kColorBookmarkBarSeparator, ThemeProperties::COLOR_BOOKMARK_SEPARATOR) \
  E(kColorBookmarkButtonIcon, ThemeProperties::COLOR_BOOKMARK_BUTTON_ICON) \
  E(kColorBookmarkFavicon, ThemeProperties::COLOR_BOOKMARK_FAVICON) \
  E_CPONLY(kColorBookmarkFolderIcon) \
  /* Window caption colors. */ \
  E(kColorCaptionButtonBackground, \
    ThemeProperties::COLOR_CONTROL_BUTTON_BACKGROUND) \
  /* Desktop media tab list colors. */ \
  E_CPONLY(kColorDesktopMediaTabListBorder) \
  E_CPONLY(kColorDesktopMediaTabListPreviewBackground) \
  /* Download shelf colors. */ \
  E(kColorDownloadShelfBackground, ThemeProperties::COLOR_DOWNLOAD_SHELF) \
  E(kColorDownloadShelfButtonBackground, \
    ThemeProperties::COLOR_DOWNLOAD_SHELF_BUTTON_BACKGROUND) \
  E(kColorDownloadShelfButtonText, \
    ThemeProperties::COLOR_DOWNLOAD_SHELF_BUTTON_TEXT) \
  E(kColorDownloadShelfContentAreaSeparator, \
    ThemeProperties::COLOR_DOWNLOAD_SHELF_CONTENT_AREA_SEPARATOR) \
  E(kColorDownloadShelfForeground, \
    ThemeProperties::COLOR_DOWNLOAD_SHELF_FOREGROUND) \
  E_CPONLY(kColorDownloadToolbarButtonActive) \
  E_CPONLY(kColorDownloadToolbarButtonInactive) \
  E_CPONLY(kColorDownloadToolbarButtonRingBackground) \
  /* Eyedropper colors. */ \
  E_CPONLY(kColorEyedropperBoundary) \
  E_CPONLY(kColorEyedropperCentralPixelInnerRing) \
  E_CPONLY(kColorEyedropperCentralPixelOuterRing) \
  E_CPONLY(kColorEyedropperGrid) \
  /* Feature Promo bubble colors. */ \
  E(kColorFeaturePromoBubbleBackground, \
    ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND) \
  E(kColorFeaturePromoBubbleButtonBorder, \
    ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BUTTON_BORDER) \
  E(kColorFeaturePromoBubbleCloseButtonInkDrop, \
    ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_CLOSE_BUTTON_INK_DROP) \
  E(kColorFeaturePromoBubbleDefaultButtonBackground, \
    ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_BACKGROUND) \
  E(kColorFeaturePromoBubbleDefaultButtonForeground, \
    ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_FOREGROUND) \
  E(kColorFeaturePromoBubbleForeground, \
    ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_FOREGROUND) \
  /* Flying Indicator colors. */ \
  E(kColorFlyingIndicatorBackground, \
    ThemeProperties::COLOR_FLYING_INDICATOR_BACKGROUND) \
  E(kColorFlyingIndicatorForeground, \
    ThemeProperties::COLOR_FLYING_INDICATOR_FOREGROUND) \
  /* Frame caption colors. */ \
  E(kColorFrameCaptionActive, ThemeProperties::COLOR_FRAME_CAPTION_ACTIVE) \
  E(kColorFrameCaptionInactive, ThemeProperties::COLOR_FRAME_CAPTION_INACTIVE) \
  /* Google branding colors. */ \
  E_CPONLY(kColorGooglePayLogo) \
  /* InfoBar colors. */ \
  E(kColorInfoBarBackground, ThemeProperties::COLOR_INFOBAR) \
  E(kColorInfoBarContentAreaSeparator, \
    ThemeProperties::COLOR_INFOBAR_CONTENT_AREA_SEPARATOR) \
  E(kColorInfoBarForeground, ThemeProperties::COLOR_INFOBAR_TEXT) \
  /* Location bar colors. */ \
  E(kColorLocationBarBorder, ThemeProperties::COLOR_LOCATION_BAR_BORDER) \
  E(kColorLocationBarBorderOpaque, \
    ThemeProperties::COLOR_LOCATION_BAR_BORDER_OPAQUE) \
  /* New Tab Page colors. */ \
  E(kColorNewTabPageBackground, ThemeProperties::COLOR_NTP_BACKGROUND) \
  E(kColorNewTabPageHeader, ThemeProperties::COLOR_NTP_HEADER) \
  E(kColorNewTabPageLink, ThemeProperties::COLOR_NTP_LINK) \
  E(kColorNewTabPageLogo, ThemeProperties::COLOR_NTP_LOGO) \
  E_CPONLY(kColorNewTabPageLogoUnthemed) \
  E(kColorNewTabPageMostVisitedTileBackground, \
    ThemeProperties::COLOR_NTP_SHORTCUT) \
  E_CPONLY(kColorNewTabPageMostVisitedTileBackgroundUnthemed) \
  E(kColorNewTabPageSectionBorder, ThemeProperties::COLOR_NTP_SECTION_BORDER) \
  E(kColorNewTabPageText, ThemeProperties::COLOR_NTP_TEXT) \
  E_CPONLY(kColorNewTabPageTextUnthemed) \
  E(kColorNewTabPageTextLight, ThemeProperties::COLOR_NTP_TEXT_LIGHT) \
  /* Omnibox colors. */ \
  E_CPONLY(kColorOmniboxAnswerIconBackground) \
  E_CPONLY(kColorOmniboxAnswerIconForeground) \
  E(kColorOmniboxBackground, ThemeProperties::COLOR_OMNIBOX_BACKGROUND) \
  E(kColorOmniboxBackgroundHovered, \
    ThemeProperties::COLOR_OMNIBOX_BACKGROUND_HOVERED) \
  E(kColorOmniboxBubbleOutline, \
    ThemeProperties::COLOR_OMNIBOX_BUBBLE_OUTLINE) \
  E(kColorOmniboxBubbleOutlineExperimentalKeywordMode, \
    ThemeProperties::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE) \
  E_CPONLY(kColorOmniboxChipBackgroundLowVisibility) \
  E_CPONLY(kColorOmniboxChipBackgroundNormalVisibility) \
  E_CPONLY(kColorOmniboxChipForegroundLowVisibility) \
  E_CPONLY(kColorOmniboxChipForegroundNormalVisibility) \
  E(kColorOmniboxKeywordSelected, \
    ThemeProperties::COLOR_OMNIBOX_SELECTED_KEYWORD) \
  E(kColorOmniboxResultsBackground, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG) \
  E(kColorOmniboxResultsBackgroundHovered, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_HOVERED) \
  E(kColorOmniboxResultsBackgroundSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_SELECTED) \
  E(kColorOmniboxResultsButtonBorder, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BUTTON_BORDER) \
  E(kColorOmniboxResultsButtonInkDrop, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP) \
  E(kColorOmniboxResultsButtonInkDropSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BUTTON_INK_DROP_SELECTED) \
  E(kColorOmniboxResultsIcon, ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON) \
  E(kColorOmniboxResultsIconSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON_SELECTED) \
  E(kColorOmniboxResultsTextDimmed, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED) \
  E(kColorOmniboxResultsTextDimmedSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED) \
  E(kColorOmniboxResultsTextNegative, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE) \
  E(kColorOmniboxResultsTextNegativeSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE_SELECTED) \
  E(kColorOmniboxResultsTextPositive, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE) \
  E(kColorOmniboxResultsTextPositiveSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE_SELECTED) \
  E(kColorOmniboxResultsTextSecondary, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY) \
  E(kColorOmniboxResultsTextSecondarySelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY_SELECTED) \
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
  /* Payments colors. */ \
  E_CPONLY(kColorPaymentRequestRowBackgroundHighlighted) \
  /* Picture-in-Picture window colors. */ \
  E_CPONLY(kColorPipWindowBackToTabButtonBackground) \
  E_CPONLY(kColorPipWindowBackground) \
  E_CPONLY(kColorPipWindowForeground) \
  E_CPONLY(kColorPipWindowHangUpButtonForeground) \
  E_CPONLY(kColorPipWindowSkipAdButtonBackground) \
  E_CPONLY(kColorPipWindowSkipAdButtonBorder) \
  /* QR code colors. */ \
  E_CPONLY(kColorQrCodeBackground) \
  E_CPONLY(kColorQrCodeBorder) \
  /* Read Later button colors. */ \
  E(kColorReadLaterButtonHighlight, \
    ThemeProperties::COLOR_READ_LATER_BUTTON_HIGHLIGHT) \
  /* Screenshot captured bubble colors. */ \
  E_CPONLY(kColorScreenshotCapturedImageBackground) \
  E_CPONLY(kColorScreenshotCapturedImageBorder) \
  /* Side panel colors. */ \
  E(kColorSidePanelContentAreaSeparator, \
    ThemeProperties::COLOR_SIDE_PANEL_CONTENT_AREA_SEPARATOR) \
  /* Status bubble colors. */ \
  E(kColorStatusBubbleBackgroundFrameActive, \
    ThemeProperties::COLOR_STATUS_BUBBLE_ACTIVE) \
  E(kColorStatusBubbleBackgroundFrameInactive, \
    ThemeProperties::COLOR_STATUS_BUBBLE_INACTIVE) \
  E_CPONLY(kColorStatusBubbleForegroundFrameActive) \
  E_CPONLY(kColorStatusBubbleForegroundFrameInactive) \
  E(kColorStatusBubbleShadow, ThemeProperties::COLOR_STATUS_BUBBLE_SHADOW) \
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
  E(kColorTabHoverCardBackground, \
    ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_BACKGROUND) \
  E(kColorTabHoverCardForeground, \
    ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_FOREGROUND) \
  /* The colors used for tab groups in the tabstrip. */ \
  E(kColorTabGroupTabStripFrameActiveGrey, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREY) \
  E(kColorTabGroupTabStripFrameActiveBlue, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_BLUE) \
  E(kColorTabGroupTabStripFrameActiveRed, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_RED) \
  E(kColorTabGroupTabStripFrameActiveYellow, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_YELLOW) \
  E(kColorTabGroupTabStripFrameActiveGreen, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREEN) \
  E(kColorTabGroupTabStripFrameActivePink, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PINK) \
  E(kColorTabGroupTabStripFrameActivePurple, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PURPLE) \
  E(kColorTabGroupTabStripFrameActiveCyan, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_CYAN) \
  E(kColorTabGroupTabStripFrameActiveOrange, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_ORANGE) \
  E(kColorTabGroupTabStripFrameInactiveGrey, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREY) \
  E(kColorTabGroupTabStripFrameInactiveBlue, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_BLUE) \
  E(kColorTabGroupTabStripFrameInactiveRed, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_RED) \
  E(kColorTabGroupTabStripFrameInactiveYellow, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_YELLOW) \
  E(kColorTabGroupTabStripFrameInactiveGreen, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREEN) \
  E(kColorTabGroupTabStripFrameInactivePink, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PINK) \
  E(kColorTabGroupTabStripFrameInactivePurple, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PURPLE) \
  E(kColorTabGroupTabStripFrameInactiveCyan, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_CYAN) \
  E(kColorTabGroupTabStripFrameInactiveOrange, \
    ThemeProperties::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_ORANGE) \
  /* The colors used for tab groups in the bubble dialog view. */ \
  E(kColorTabGroupDialogGrey, ThemeProperties::COLOR_TAB_GROUP_DIALOG_GREY) \
  E(kColorTabGroupDialogBlue, ThemeProperties::COLOR_TAB_GROUP_DIALOG_BLUE) \
  E(kColorTabGroupDialogRed, ThemeProperties::COLOR_TAB_GROUP_DIALOG_RED) \
  E(kColorTabGroupDialogYellow, \
    ThemeProperties::COLOR_TAB_GROUP_DIALOG_YELLOW) \
  E(kColorTabGroupDialogGreen, ThemeProperties::COLOR_TAB_GROUP_DIALOG_GREEN) \
  E(kColorTabGroupDialogPink, ThemeProperties::COLOR_TAB_GROUP_DIALOG_PINK) \
  E(kColorTabGroupDialogPurple, \
    ThemeProperties::COLOR_TAB_GROUP_DIALOG_PURPLE) \
  E(kColorTabGroupDialogCyan, ThemeProperties::COLOR_TAB_GROUP_DIALOG_CYAN) \
  E(kColorTabGroupDialogOrange, \
    ThemeProperties::COLOR_TAB_GROUP_DIALOG_ORANGE) \
  /* The colors used for tab groups in the context submenu. */ \
  E(kColorTabGroupContextMenuBlue, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_BLUE) \
  E(kColorTabGroupContextMenuCyan, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_CYAN) \
  E(kColorTabGroupContextMenuGreen, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_GREEN) \
  E(kColorTabGroupContextMenuGrey, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_GREY) \
  E(kColorTabGroupContextMenuOrange, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_ORANGE) \
  E(kColorTabGroupContextMenuPink, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_PINK) \
  E(kColorTabGroupContextMenuPurple, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_PURPLE) \
  E(kColorTabGroupContextMenuRed, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_RED) \
  E(kColorTabGroupContextMenuYellow, \
    ThemeProperties::COLOR_TAB_GROUP_CONTEXT_MENU_YELLOW) \
  /* The colors used for saved tab group chips on the bookmark bar. */ \
  E(kColorTabGroupBookmarkBarGrey, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_GREY) \
  E(kColorTabGroupBookmarkBarBlue, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_BLUE) \
  E(kColorTabGroupBookmarkBarRed, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_RED) \
  E(kColorTabGroupBookmarkBarYellow, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_YELLOW) \
  E(kColorTabGroupBookmarkBarGreen, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_GREEN) \
  E(kColorTabGroupBookmarkBarPink, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_PINK) \
  E(kColorTabGroupBookmarkBarPurple, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_PURPLE) \
  E(kColorTabGroupBookmarkBarCyan, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_CYAN) \
  E(kColorTabGroupBookmarkBarOrange, \
    ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_ORANGE) \
  E(kColorTabStrokeFrameActive, \
    ThemeProperties::COLOR_TAB_STROKE_FRAME_ACTIVE) \
  E(kColorTabStrokeFrameInactive, \
    ThemeProperties::COLOR_TAB_STROKE_FRAME_INACTIVE) \
  /* Thumbnail tab colors. */ \
  E_CPONLY(kColorThumbnailTabBackground) \
  E_CPONLY(kColorThumbnailTabForeground) \
  /* Toolbar colors. */ \
  E(kColorToolbar, ThemeProperties::COLOR_TOOLBAR) \
  E(kColorToolbarButtonBackground, \
    ThemeProperties::COLOR_TOOLBAR_BUTTON_BACKGROUND) \
  E(kColorToolbarButtonBorder, ThemeProperties::COLOR_TOOLBAR_BUTTON_BORDER) \
  E(kColorToolbarButtonIcon, ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON) \
  E(kColorToolbarButtonIconHovered, \
    ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_HOVERED) \
  E(kColorToolbarButtonIconInactive, \
    ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE) \
  E(kColorToolbarButtonIconPressed, \
    ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_PRESSED) \
  E(kColorToolbarButtonText, ThemeProperties::COLOR_TOOLBAR_BUTTON_TEXT) \
  E(kColorToolbarContentAreaSeparator, \
    ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR) \
  E(kColorToolbarFeaturePromoHighlight, \
    ThemeProperties::COLOR_TOOLBAR_FEATURE_PROMO_HIGHLIGHT) \
  E(kColorToolbarInkDrop, ThemeProperties::COLOR_TOOLBAR_INK_DROP) \
  E(kColorToolbarSeparator, \
    ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR) \
  E(kColorToolbarText, ThemeProperties::COLOR_TOOLBAR_TEXT) \
  E(kColorToolbarTopSeparatorFrameActive, \
    ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE) \
  E(kColorToolbarTopSeparatorFrameInactive, \
    ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE) \
  /* Window control button background colors */ \
  E(kColorWindowControlButtonBackgroundActive, \
    ThemeProperties::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE) \
  E(kColorWindowControlButtonBackgroundInactive, \
    ThemeProperties::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE)

#if BUILDFLAG(IS_CHROMEOS)
#define CHROME_NATIVE_COLOR_IDS \
    /* Caption colors. */ \
    E_CPONLY(kColorCaptionForeground)
#elif BUILDFLAG(IS_WIN)
#define CHROME_NATIVE_COLOR_IDS \
    /* The colors of the 1px border around the window on Windows 10. */ \
    E(kColorAccentBorderActive, ThemeProperties::COLOR_ACCENT_BORDER_ACTIVE) \
    E(kColorAccentBorderInactive, \
      ThemeProperties::COLOR_ACCENT_BORDER_INACTIVE) \
    /* Caption colors. */ \
    E_CPONLY(kColorCaptionButtonForegroundActive) \
    E_CPONLY(kColorCaptionButtonForegroundInactive) \
    E_CPONLY(kColorCaptionCloseButtonBackgroundHovered) \
    E_CPONLY(kColorCaptionCloseButtonForegroundHovered) \
    E_CPONLY(kColorCaptionForegroundActive) \
    E_CPONLY(kColorCaptionForegroundInactive)
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
