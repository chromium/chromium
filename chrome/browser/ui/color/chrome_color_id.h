// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_

#include "chrome/browser/themes/theme_properties.h"
#include "ui/color/color_id.h"

// TODO(pkasting): Add the rest of the colors.

// clang-format off
#define COMMON_CHROME_COLOR_IDS \
  /* App menu colors. */ \
  /* The kColorAppMenuHighlightSeverityLow color id is used in */ \
  /* color_provider_css_colors_test.ts. If changing the variable name, the */ \
  /* variable name in the test needs to be changed as well. */ \
  E_CPONLY(kColorAppMenuHighlightSeverityLow, kChromeColorsStart, \
           kChromeColorsStart) \
  E_CPONLY(kColorAppMenuHighlightSeverityHigh) \
  E_CPONLY(kColorAppMenuHighlightSeverityMedium) \
  /* Avatar colors. */ \
  E_CPONLY(kColorAvatarButtonHighlightNormal) \
  E_CPONLY(kColorAvatarButtonHighlightSyncError) \
  E_CPONLY(kColorAvatarButtonHighlightSyncPaused) \
  E_CPONLY(kColorAvatarStrokeLight) \
  /* Bookmark bar colors. */ \
  E(kColorBookmarkBarBackground, \
    ThemeProperties::COLOR_BOOKMARK_BAR_BACKGROUND) \
  E(kColorBookmarkBarForeground, ThemeProperties::COLOR_BOOKMARK_TEXT) \
  E(kColorBookmarkBarSeparator, ThemeProperties::COLOR_BOOKMARK_SEPARATOR) \
  E_CPONLY(kColorBookmarkButtonIcon) \
  E_CPONLY(kColorBookmarkDragImageBackground) \
  E_CPONLY(kColorBookmarkDragImageCountBackground) \
  E_CPONLY(kColorBookmarkDragImageCountForeground) \
  E_CPONLY(kColorBookmarkDragImageForeground) \
  E_CPONLY(kColorBookmarkDragImageIconBackground) \
  E(kColorBookmarkFavicon, ThemeProperties::COLOR_BOOKMARK_FAVICON) \
  E_CPONLY(kColorBookmarkFolderIcon) \
  /* Window caption colors. */ \
  E(kColorCaptionButtonBackground, \
    ThemeProperties::COLOR_CONTROL_BUTTON_BACKGROUND) \
  /* Captured tab colors. */ \
  E_CPONLY(kColorCapturedTabContentsBorder) \
  /* Desktop media tab list colors. */ \
  E_CPONLY(kColorDesktopMediaTabListBorder) \
  E_CPONLY(kColorDesktopMediaTabListPreviewBackground) \
  /* Download shelf colors. */ \
  E_CPONLY(kColorDownloadItemForeground) \
  E_CPONLY(kColorDownloadItemForegroundDangerous) \
  E_CPONLY(kColorDownloadItemForegroundDisabled) \
  E_CPONLY(kColorDownloadItemForegroundSafe) \
  E_CPONLY(kColorDownloadItemProgressRingBackground) \
  E_CPONLY(kColorDownloadItemProgressRingForeground) \
  E_CPONLY(kColorDownloadShelfBackground) \
  E_CPONLY(kColorDownloadShelfButtonBackground) \
  E_CPONLY(kColorDownloadShelfButtonText) \
  E_CPONLY(kColorDownloadShelfButtonIcon) \
  E_CPONLY(kColorDownloadShelfButtonIconDisabled) \
  E_CPONLY(kColorDownloadShelfContentAreaSeparator) \
  E_CPONLY(kColorDownloadShelfForeground) \
  E_CPONLY(kColorDownloadStartedAnimationForeground) \
  E_CPONLY(kColorDownloadToolbarButtonActive) \
  E_CPONLY(kColorDownloadToolbarButtonInactive) \
  E_CPONLY(kColorDownloadToolbarButtonRingBackground) \
  /* Extension colors. */ \
  E_CPONLY(kColorExtensionDialogBackground) \
  E_CPONLY(kColorExtensionIconBadgeBackgroundDefault) \
  E_CPONLY(kColorExtensionIconDecorationAmbientShadow) \
  E_CPONLY(kColorExtensionIconDecorationBackground) \
  E_CPONLY(kColorExtensionIconDecorationKeyShadow) \
  E_CPONLY(kColorExtensionMenuIcon) \
  E_CPONLY(kColorExtensionMenuIconDisabled) \
  E_CPONLY(kColorExtensionMenuPinButtonIcon) \
  E_CPONLY(kColorExtensionMenuPinButtonIconDisabled) \
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
  /* Find bar colors. */ \
  E_CPONLY(kColorFindBarBackground) \
  E_CPONLY(kColorFindBarButtonIcon) \
  E_CPONLY(kColorFindBarButtonIconDisabled) \
  E_CPONLY(kColorFindBarForeground) \
  E_CPONLY(kColorFindBarMatchCount) \
  E_CPONLY(kColorFindBarSeparator) \
  /* Flying Indicator colors. */ \
  E(kColorFlyingIndicatorBackground, \
    ThemeProperties::COLOR_FLYING_INDICATOR_BACKGROUND) \
  E(kColorFlyingIndicatorForeground, \
    ThemeProperties::COLOR_FLYING_INDICATOR_FOREGROUND) \
  /* Default accessibility focus highlight. */ \
  E_CPONLY(kColorFocusHighlightDefault) \
  /* Frame caption colors. */ \
  E(kColorFrameCaptionActive, ThemeProperties::COLOR_FRAME_CAPTION_ACTIVE) \
  E(kColorFrameCaptionInactive, ThemeProperties::COLOR_FRAME_CAPTION_INACTIVE) \
  /* InfoBar colors. */ \
  E_CPONLY(kColorInfoBarBackground) \
  E_CPONLY(kColorInfoBarButtonIcon) \
  E_CPONLY(kColorInfoBarButtonIconDisabled) \
  E_CPONLY(kColorInfoBarContentAreaSeparator) \
  E_CPONLY(kColorInfoBarForeground) \
  /* Intent Picker colors. */ \
  E_CPONLY(kColorIntentPickerItemBackgroundHovered) \
  E_CPONLY(kColorIntentPickerItemBackgroundSelected) \
  /* Location bar colors. */ \
  E(kColorLocationBarBorder, ThemeProperties::COLOR_LOCATION_BAR_BORDER) \
  E(kColorLocationBarBorderOpaque, \
    ThemeProperties::COLOR_LOCATION_BAR_BORDER_OPAQUE) \
  E_CPONLY(kColorLocationBarClearAllButtonIcon) \
  E_CPONLY(kColorLocationBarClearAllButtonIconDisabled) \
  /* Media router colors. */ \
  E_CPONLY(kColorMediaRouterIconActive) \
  E_CPONLY(kColorMediaRouterIconError) \
  E_CPONLY(kColorMediaRouterIconWarning) \
  /* New tab button colors. */ \
  E_CPONLY(kColorNewTabButtonBackgroundFrameActive) \
  E_CPONLY(kColorNewTabButtonBackgroundFrameInactive) \
  E_CPONLY(kColorNewTabButtonFocusRing) \
  E_CPONLY(kColorNewTabButtonInkDropFrameActive) \
  E_CPONLY(kColorNewTabButtonInkDropFrameInactive) \
  /* New Tab Page colors. */ \
  E(kColorNewTabPageBackground, ThemeProperties::COLOR_NTP_BACKGROUND) \
  E(kColorNewTabPageHeader, ThemeProperties::COLOR_NTP_HEADER) \
  E(kColorNewTabPageLink, ThemeProperties::COLOR_NTP_LINK) \
  E(kColorNewTabPageLogo, ThemeProperties::COLOR_NTP_LOGO) \
  E_CPONLY(kColorNewTabPageLogoUnthemedDark) \
  E_CPONLY(kColorNewTabPageLogoUnthemedLight) \
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
  E_CPONLY(kColorOmniboxBackground) \
  E_CPONLY(kColorOmniboxBackgroundHovered) \
  E_CPONLY(kColorOmniboxBubbleOutline) \
  E_CPONLY(kColorOmniboxBubbleOutlineExperimentalKeywordMode) \
  E_CPONLY(kColorOmniboxChipBackgroundLowVisibility) \
  E_CPONLY(kColorOmniboxChipBackgroundNormalVisibility) \
  E_CPONLY(kColorOmniboxChipForegroundLowVisibility) \
  E_CPONLY(kColorOmniboxChipForegroundNormalVisibility) \
  E_CPONLY(kColorOmniboxKeywordSelected) \
  E_CPONLY(kColorOmniboxResultsBackground) \
  E_CPONLY(kColorOmniboxResultsBackgroundHovered) \
  E_CPONLY(kColorOmniboxResultsBackgroundSelected) \
  E_CPONLY(kColorOmniboxResultsButtonBorder) \
  E_CPONLY(kColorOmniboxResultsButtonInkDrop) \
  E_CPONLY(kColorOmniboxResultsButtonInkDropSelected) \
  E_CPONLY(kColorOmniboxResultsIcon) \
  E_CPONLY(kColorOmniboxResultsIconSelected) \
  E_CPONLY(kColorOmniboxResultsTextDimmed) \
  E_CPONLY(kColorOmniboxResultsTextDimmedSelected) \
  E_CPONLY(kColorOmniboxResultsTextNegative) \
  E_CPONLY(kColorOmniboxResultsTextNegativeSelected) \
  E_CPONLY(kColorOmniboxResultsTextPositive) \
  E_CPONLY(kColorOmniboxResultsTextPositiveSelected) \
  E_CPONLY(kColorOmniboxResultsTextSecondary) \
  E_CPONLY(kColorOmniboxResultsTextSecondarySelected) \
  E_CPONLY(kColorOmniboxResultsTextSelected) \
  E_CPONLY(kColorOmniboxResultsUrl) \
  E_CPONLY(kColorOmniboxResultsUrlSelected) \
  E_CPONLY(kColorOmniboxSecurityChipDangerous) \
  E_CPONLY(kColorOmniboxSecurityChipDefault) \
  E_CPONLY(kColorOmniboxSecurityChipSecure) \
  E_CPONLY(kColorOmniboxText) \
  E_CPONLY(kColorOmniboxTextDimmed) \
  /* Page Info colors */ \
  E_CPONLY(kColorPageInfoChosenObjectDeleteButtonIcon) \
  E_CPONLY(kColorPageInfoChosenObjectDeleteButtonIconDisabled) \
  /* Payments colors. */ \
  E_CPONLY(kColorPaymentsFeedbackTipBackground) \
  E_CPONLY(kColorPaymentsFeedbackTipBorder) \
  E_CPONLY(kColorPaymentsFeedbackTipForeground) \
  E_CPONLY(kColorPaymentsFeedbackTipIcon) \
  E_CPONLY(kColorPaymentsGooglePayLogo) \
  E_CPONLY(kColorPaymentsPromoCodeBackground) \
  E_CPONLY(kColorPaymentsPromoCodeForeground) \
  E_CPONLY(kColorPaymentsPromoCodeForegroundHovered) \
  E_CPONLY(kColorPaymentsPromoCodeForegroundPressed) \
  E_CPONLY(kColorPaymentsPromoCodeInkDrop) \
  E_CPONLY(kColorPaymentsRequestBackArrowButtonIcon) \
  E_CPONLY(kColorPaymentsRequestBackArrowButtonIconDisabled) \
  E_CPONLY(kColorPaymentsRequestRowBackgroundHighlighted) \
  /* Picture-in-Picture window colors. */ \
  E_CPONLY(kColorPipWindowBackToTabButtonBackground) \
  E_CPONLY(kColorPipWindowBackground) \
  E_CPONLY(kColorPipWindowControlsBackground) \
  E_CPONLY(kColorPipWindowForeground) \
  E_CPONLY(kColorPipWindowHangUpButtonForeground) \
  E_CPONLY(kColorPipWindowSkipAdButtonBackground) \
  E_CPONLY(kColorPipWindowSkipAdButtonBorder) \
  /* Profiles colors. */ \
  E_CPONLY(kColorProfilesReauthDialogBorder) \
  /* PWA colors. */ \
  E_CPONLY(kColorPwaBackground) \
  E_CPONLY(kColorPwaMenuButtonIcon) \
  E_CPONLY(kColorPwaSecurityChipForeground) \
  E_CPONLY(kColorPwaSecurityChipForegroundDangerous) \
  E_CPONLY(kColorPwaSecurityChipForegroundPolicyCert) \
  E_CPONLY(kColorPwaSecurityChipForegroundSecure) \
  E_CPONLY(kColorPwaTabBarBottomSeparator) \
  E_CPONLY(kColorPwaTabBarTopSeparator) \
  E_CPONLY(kColorPwaTheme) \
  E_CPONLY(kColorPwaToolbarBackground) \
  E_CPONLY(kColorPwaToolbarButtonIcon) \
  E_CPONLY(kColorPwaToolbarButtonIconDisabled) \
  /* QR code colors. */ \
  E_CPONLY(kColorQrCodeBackground) \
  E_CPONLY(kColorQrCodeBorder) \
  /* Quick Answers colors. */ \
  E_CPONLY(kColorQuickAnswersReportQueryButtonBackground) \
  E_CPONLY(kColorQuickAnswersReportQueryButtonForeground) \
  /* Screenshot captured bubble colors. */ \
  E_CPONLY(kColorScreenshotCapturedImageBackground) \
  E_CPONLY(kColorScreenshotCapturedImageBorder) \
  /* Side panel colors. */ \
  E_CPONLY(kColorSidePanelBackground) \
  E_CPONLY(kColorSidePanelContentAreaSeparator) \
  /* Status bubble colors. */ \
  E_CPONLY(kColorStatusBubbleBackgroundFrameActive) \
  E_CPONLY(kColorStatusBubbleBackgroundFrameInactive) \
  E_CPONLY(kColorStatusBubbleForegroundFrameActive) \
  E_CPONLY(kColorStatusBubbleForegroundFrameInactive) \
  E_CPONLY(kColorStatusBubbleShadow) \
  /* Tab alert colors. */ \
  E_CPONLY(kColorTabAlertAudioPlayingActiveFrameActive) \
  E_CPONLY(kColorTabAlertAudioPlayingActiveFrameInactive) \
  E_CPONLY(kColorTabAlertAudioPlayingInactiveFrameActive) \
  E_CPONLY(kColorTabAlertAudioPlayingInactiveFrameInactive) \
  E_CPONLY(kColorTabAlertMediaRecordingActiveFrameActive) \
  E_CPONLY(kColorTabAlertMediaRecordingActiveFrameInactive) \
  E_CPONLY(kColorTabAlertMediaRecordingInactiveFrameActive) \
  E_CPONLY(kColorTabAlertMediaRecordingInactiveFrameInactive) \
  E_CPONLY(kColorTabAlertPipPlayingActiveFrameActive) \
  E_CPONLY(kColorTabAlertPipPlayingActiveFrameInactive) \
  E_CPONLY(kColorTabAlertPipPlayingInactiveFrameActive) \
  E_CPONLY(kColorTabAlertPipPlayingInactiveFrameInactive) \
  /* Tab colors. */ \
  E(kColorTabBackgroundActiveFrameActive, \
    ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_ACTIVE) \
  E(kColorTabBackgroundActiveFrameInactive, \
    ThemeProperties::COLOR_TAB_BACKGROUND_ACTIVE_FRAME_INACTIVE) \
  E(kColorTabBackgroundInactiveFrameActive, \
    ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_ACTIVE) \
  E(kColorTabBackgroundInactiveFrameInactive, \
    ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE) \
  E_CPONLY(kColorTabCloseButtonFocusRingActive) \
  E_CPONLY(kColorTabCloseButtonFocusRingInactive) \
  E_CPONLY(kColorTabFocusRingActive) \
  E_CPONLY(kColorTabFocusRingInactive) \
  E_CPONLY(kColorTabForegroundActiveFrameActive) \
  E_CPONLY(kColorTabForegroundActiveFrameInactive) \
  E_CPONLY(kColorTabForegroundInactiveFrameActive) \
  E_CPONLY(kColorTabForegroundInactiveFrameInactive) \
  E_CPONLY(kColorTabHoverCardBackground) \
  E_CPONLY(kColorTabHoverCardForeground) \
  /* The colors used for tab groups in the tabstrip. */ \
  E_CPONLY(kColorTabGroupTabStripFrameActiveGrey) \
  E_CPONLY(kColorTabGroupTabStripFrameActiveBlue) \
  E_CPONLY(kColorTabGroupTabStripFrameActiveRed) \
  E_CPONLY(kColorTabGroupTabStripFrameActiveYellow) \
  E_CPONLY(kColorTabGroupTabStripFrameActiveGreen) \
  E_CPONLY(kColorTabGroupTabStripFrameActivePink) \
  E_CPONLY(kColorTabGroupTabStripFrameActivePurple) \
  E_CPONLY(kColorTabGroupTabStripFrameActiveCyan) \
  E_CPONLY(kColorTabGroupTabStripFrameActiveOrange) \
  E_CPONLY(kColorTabGroupTabStripFrameInactiveGrey) \
  E_CPONLY(kColorTabGroupTabStripFrameInactiveBlue) \
  E_CPONLY(kColorTabGroupTabStripFrameInactiveRed) \
  E_CPONLY(kColorTabGroupTabStripFrameInactiveYellow) \
  E_CPONLY(kColorTabGroupTabStripFrameInactiveGreen) \
  E_CPONLY(kColorTabGroupTabStripFrameInactivePink) \
  E_CPONLY(kColorTabGroupTabStripFrameInactivePurple) \
  E_CPONLY(kColorTabGroupTabStripFrameInactiveCyan) \
  E_CPONLY(kColorTabGroupTabStripFrameInactiveOrange) \
  /* The colors used for tab groups in the bubble dialog view. */ \
  E_CPONLY(kColorTabGroupDialogGrey) \
  E_CPONLY(kColorTabGroupDialogBlue) \
  E_CPONLY(kColorTabGroupDialogRed) \
  E_CPONLY(kColorTabGroupDialogYellow) \
  E_CPONLY(kColorTabGroupDialogGreen) \
  E_CPONLY(kColorTabGroupDialogPink) \
  E_CPONLY(kColorTabGroupDialogPurple) \
  E_CPONLY(kColorTabGroupDialogCyan) \
  E_CPONLY(kColorTabGroupDialogOrange) \
  /* The colors used for tab groups in the context submenu. */ \
  E_CPONLY(kColorTabGroupContextMenuBlue) \
  E_CPONLY(kColorTabGroupContextMenuCyan) \
  E_CPONLY(kColorTabGroupContextMenuGreen) \
  E_CPONLY(kColorTabGroupContextMenuGrey) \
  E_CPONLY(kColorTabGroupContextMenuOrange) \
  E_CPONLY(kColorTabGroupContextMenuPink) \
  E_CPONLY(kColorTabGroupContextMenuPurple) \
  E_CPONLY(kColorTabGroupContextMenuRed) \
  E_CPONLY(kColorTabGroupContextMenuYellow) \
  /* The colors used for saved tab group chips on the bookmark bar. */ \
  E_CPONLY(kColorTabGroupBookmarkBarGrey) \
  E_CPONLY(kColorTabGroupBookmarkBarBlue) \
  E_CPONLY(kColorTabGroupBookmarkBarRed) \
  E_CPONLY(kColorTabGroupBookmarkBarYellow) \
  E_CPONLY(kColorTabGroupBookmarkBarGreen) \
  E_CPONLY(kColorTabGroupBookmarkBarPink) \
  E_CPONLY(kColorTabGroupBookmarkBarPurple) \
  E_CPONLY(kColorTabGroupBookmarkBarCyan) \
  E_CPONLY(kColorTabGroupBookmarkBarOrange) \
  E(kColorTabStrokeFrameActive, \
    ThemeProperties::COLOR_TAB_STROKE_FRAME_ACTIVE) \
  E(kColorTabStrokeFrameInactive, \
    ThemeProperties::COLOR_TAB_STROKE_FRAME_INACTIVE) \
  E_CPONLY(kColorTabstripLoadingProgressBackground) \
  E_CPONLY(kColorTabstripLoadingProgressForeground) \
  E_CPONLY(kColorTabstripScrollContainerShadow) \
  E_CPONLY(kColorTabThrobber) \
  E_CPONLY(kColorTabThrobberPreconnect) \
  /* Thumbnail tab colors. */ \
  E_CPONLY(kColorThumbnailTabBackground) \
  E_CPONLY(kColorThumbnailTabForeground) \
  E_CPONLY(kColorThumbnailTabStripBackgroundActive) \
  E_CPONLY(kColorThumbnailTabStripBackgroundInactive) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActiveGrey) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActiveBlue) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActiveRed) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActiveYellow) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActiveGreen) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActivePink) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActivePurple) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActiveCyan) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameActiveOrange) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactiveGrey) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactiveBlue) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactiveRed) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactiveYellow) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactiveGreen) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactivePink) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactivePurple) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactiveCyan) \
  E_CPONLY(kColorThumbnailTabStripTabGroupFrameInactiveOrange) \
  /* Toolbar colors. */ \
  E(kColorToolbar, ThemeProperties::COLOR_TOOLBAR) \
  E_CPONLY(kColorToolbarButtonBackgroundHighlightedDefault) \
  E(kColorToolbarButtonBorder, ThemeProperties::COLOR_TOOLBAR_BUTTON_BORDER) \
  E(kColorToolbarButtonIcon, ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON) \
  E_CPONLY(kColorToolbarButtonIconDefault) \
  E_CPONLY(kColorToolbarButtonIconDisabled) \
  E(kColorToolbarButtonIconHovered, \
    ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_HOVERED) \
  E(kColorToolbarButtonIconInactive, \
    ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE) \
  E(kColorToolbarButtonIconPressed, \
    ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_PRESSED) \
  E(kColorToolbarButtonText, ThemeProperties::COLOR_TOOLBAR_BUTTON_TEXT) \
  E(kColorToolbarContentAreaSeparator, \
    ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR) \
  E_CPONLY(kColorToolbarFeaturePromoHighlight) \
  E(kColorToolbarInkDrop, ThemeProperties::COLOR_TOOLBAR_INK_DROP) \
  E(kColorToolbarSeparator, \
    ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR) \
  E_CPONLY(kColorToolbarSeparatorDefault) \
  E(kColorToolbarText, ThemeProperties::COLOR_TOOLBAR_TEXT) \
  E_CPONLY(kColorToolbarTextDefault) \
  E_CPONLY(kColorToolbarTextDisabled) \
  E_CPONLY(kColorToolbarTextDisabledDefault) \
  E(kColorToolbarTopSeparatorFrameActive, \
    ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_ACTIVE) \
  E(kColorToolbarTopSeparatorFrameInactive, \
    ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE) \
  /* WebAuthn colors. */ \
  E_CPONLY(kColorWebAuthnBackArrowButtonIcon) \
  E_CPONLY(kColorWebAuthnBackArrowButtonIconDisabled) \
  E_CPONLY(kColorWebAuthnPinTextfieldBottomBorder) \
  E_CPONLY(kColorWebAuthnProgressRingBackground) \
  E_CPONLY(kColorWebAuthnProgressRingForeground) \
  /* Web contents colors. */ \
  E_CPONLY(kColorWebContentsBackground) \
  E_CPONLY(kColorWebContentsBackgroundLetterboxing) \
  /* Window control button background colors. */ \
  E(kColorWindowControlButtonBackgroundActive, \
    ThemeProperties::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE) \
  E(kColorWindowControlButtonBackgroundInactive, \
    ThemeProperties::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE)

#if BUILDFLAG(IS_CHROMEOS)
#define CHROME_PLATFORM_SPECIFIC_COLOR_IDS \
    /* Borealis colors. */ \
    E_CPONLY(kColorBorealisSplashScreenBackground) \
    E_CPONLY(kColorBorealisSplashScreenForeground) \
    /* Caption colors. */ \
    E_CPONLY(kColorCaptionForeground) \
    /* Sharesheet colors. */ \
    E_CPONLY(kColorSharesheetTargetButtonIconShadow)
#elif BUILDFLAG(IS_WIN)
#define CHROME_PLATFORM_SPECIFIC_COLOR_IDS \
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
    E_CPONLY(kColorCaptionForegroundInactive) \
    /* Tab search caption button colors. */ \
    E_CPONLY(kColorTabSearchCaptionButtonFocusRing) \
    /* Try Chrome dialog colors. */ \
    E_CPONLY(kColorTryChromeBackground) \
    E_CPONLY(kColorTryChromeBorder) \
    E_CPONLY(kColorTryChromeButtonBackgroundAccept) \
    E_CPONLY(kColorTryChromeButtonBackgroundNoThanks) \
    E_CPONLY(kColorTryChromeButtonForeground) \
    E_CPONLY(kColorTryChromeForeground) \
    E_CPONLY(kColorTryChromeHeaderForeground)
#else
#define CHROME_PLATFORM_SPECIFIC_COLOR_IDS
#endif  // BUILDFLAG(IS_WIN)

#define CHROME_COLOR_IDS \
    COMMON_CHROME_COLOR_IDS CHROME_PLATFORM_SPECIFIC_COLOR_IDS

#include "ui/color/color_id_macros.inc"

enum ChromeColorIds : ui::ColorId {
  kChromeColorsStart = ui::kUiColorsEnd,

  CHROME_COLOR_IDS

  kChromeColorsEnd,
};

#include "ui/color/color_id_macros.inc"

// clang-format on

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
