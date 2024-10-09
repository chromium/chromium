// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_

#include "components/color/color_id.h"
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
  E_CPONLY(kColorAppMenuHighlightDefault) \
  E_CPONLY(kColorAppMenuHighlightPrimary) \
  E_CPONLY(kColorAppMenuExpandedForegroundDefault) \
  E_CPONLY(kColorAppMenuExpandedForegroundPrimary) \
  E_CPONLY(kColorAppMenuChipInkDropHover) \
  E_CPONLY(kColorAppMenuChipInkDropRipple) \
  /* Content settings activity indicators popup dialog colors */ \
  E_CPONLY(kColorActivityIndicatorForeground) \
  E_CPONLY(kColorActivityIndicatorSubtitleForeground) \
  /* Avatar colors. */ \
  /* TODO(crbug.com/40259490): Refactor the Avatar Button colors as Profile */ \
  /* Menu Button colors. */ \
  E_CPONLY(kColorAvatarButtonHighlightDefault) \
  E_CPONLY(kColorAvatarButtonHighlightNormal) \
  E_CPONLY(kColorAvatarButtonHighlightSyncError) \
  E_CPONLY(kColorAvatarButtonHighlightSyncPaused) \
  E_CPONLY(kColorAvatarButtonHighlightSigninPaused) \
  E_CPONLY(kColorAvatarButtonHighlightExplicitText) \
  E_CPONLY(kColorAvatarButtonHighlightIncognito) \
  E_CPONLY(kColorAvatarButtonHighlightNormalForeground) \
  E_CPONLY(kColorAvatarButtonHighlightDefaultForeground) \
  E_CPONLY(kColorAvatarButtonHighlightSyncErrorForeground) \
  E_CPONLY(kColorAvatarButtonHighlightIncognitoForeground) \
  E_CPONLY(kColorAvatarButtonIncognitoHover) \
  E_CPONLY(kColorAvatarButtonNormalRipple) \
  E_CPONLY(kColorAvatarStrokeLight) \
  E_CPONLY(kColorAvatarStroke) \
  E_CPONLY(kColorAvatarFillForContrast) \
  /* Bookmark bar colors. */ \
  E_CPONLY(kColorBookmarkBarBackground) \
  E_CPONLY(kColorBookmarkBarForeground) \
  E_CPONLY(kColorBookmarkBarForegroundDisabled) \
  E_CPONLY(kColorBookmarkBarSeparator) \
  E_CPONLY(kColorBookmarkBarSeparatorChromeRefresh) \
  E_CPONLY(kColorBookmarkButtonIcon) \
  E_CPONLY(kColorBookmarkDialogTrackPriceIcon) \
  E_CPONLY(kColorBookmarkDialogProductImageBorder) \
  E_CPONLY(kColorBookmarkDragImageBackground) \
  E_CPONLY(kColorBookmarkDragImageCountBackground) \
  E_CPONLY(kColorBookmarkDragImageCountForeground) \
  E_CPONLY(kColorBookmarkDragImageForeground) \
  E_CPONLY(kColorBookmarkDragImageIconBackground) \
  E_CPONLY(kColorBookmarkFavicon) \
  E_CPONLY(kColorBookmarkFolderIcon) \
  /* Window caption colors. */ \
  E_CPONLY(kColorCaptionButtonBackground) \
  /* Captured tab colors. */ \
  E_CPONLY(kColorCapturedTabContentsBorder) \
  /* Cast dialog colors. */ \
  E_CPONLY(kColorCastDialogHelpIcon) \
  /* Signin bubble colors. */ \
  E_CPONLY(kColorChromeSigninBubbleBackground) \
  E_CPONLY(kColorChromeSigninBubbleInfoBackground) \
  /* Batch Upload colors. */ \
  E_CPONLY(kColorBatchUploadBackground) \
  E_CPONLY(kColorBatchUploadDataBackground) \
  /* Compose colors */ \
  E_CPONLY(kColorComposeDialogBackground) \
  E_CPONLY(kColorComposeDialogDivider) \
  E_CPONLY(kColorComposeDialogError) \
  E_CPONLY(kColorComposeDialogForegroundSubtle) \
  E_CPONLY(kColorComposeDialogLink) \
  E_CPONLY(kColorComposeDialogLogo) \
  E_CPONLY(kColorComposeDialogResultBackground) \
  E_CPONLY(kColorComposeDialogResultForeground) \
  E_CPONLY(kColorComposeDialogResultForegroundWhileLoading) \
  E_CPONLY(kColorComposeDialogResultIcon) \
  E_CPONLY(kColorComposeDialogResultContainerScrollbarThumb) \
  E_CPONLY(kColorComposeDialogScrollbarThumb) \
  E_CPONLY(kColorComposeDialogTitle) \
  E_CPONLY(kColorComposeDialogTextarea) \
  E_CPONLY(kColorComposeDialogTextareaOutline) \
  E_CPONLY(kColorComposeDialogTextareaPlaceholder) \
  E_CPONLY(kColorComposeDialogTextareaReadonlyBackground) \
  E_CPONLY(kColorComposeDialogTextareaReadonlyForeground) \
  E_CPONLY(kColorComposeDialogTextareaIcon) \
  E_CPONLY(kColorComposeDialogSelectOptionDisabled) \
  /* Desktop media picker view. */ \
  E_CPONLY(kColorDesktopMediaPickerDescriptionLabel) \
  /* Desktop media tab list colors. */ \
  E_CPONLY(kColorDesktopMediaTabListBorder) \
  E_CPONLY(kColorDesktopMediaTabListPreviewBackground) \
  /* Common Download colors. */ \
  E_CPONLY(kColorDownloadItemIconDangerous) \
  E_CPONLY(kColorDownloadItemTextDangerous) \
  E_CPONLY(kColorDownloadItemIconWarning) \
  E_CPONLY(kColorDownloadItemTextWarning) \
  /* Download bubble colors. */\
  E_CPONLY(kColorDownloadBubbleInfoBackground) \
  E_CPONLY(kColorDownloadBubbleInfoIcon) \
  E_CPONLY(kColorDownloadBubbleRowHover) \
  E_CPONLY(kColorDownloadBubbleShowAllDownloadsIcon) \
  E_CPONLY(kColorDownloadBubblePrimaryIcon) \
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
  E_CPONLY(kColorDownloadToolbarButtonAnimationBackground) \
  E_CPONLY(kColorDownloadToolbarButtonAnimationForeground) \
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
  E_CPONLY(kColorExtensionsMenuContainerBackground) \
  E_CPONLY(kColorExtensionsToolbarControlsBackground) \
  E_CPONLY(kColorExtensionsMenuText) \
  E_CPONLY(kColorExtensionsMenuSecondaryText) \
  /* Feature Promo bubble colors. */ \
  E_CPONLY(kColorFeaturePromoBubbleBackground) \
  E_CPONLY(kColorFeaturePromoBubbleButtonBorder) \
  E_CPONLY(kColorFeaturePromoBubbleCloseButtonInkDrop) \
  E_CPONLY(kColorFeaturePromoBubbleDefaultButtonBackground) \
  E_CPONLY(kColorFeaturePromoBubbleDefaultButtonForeground) \
  E_CPONLY(kColorFeaturePromoBubbleForeground) \
  E_CPONLY(kColorFeatureLensPromoBubbleBackground) \
  E_CPONLY(kColorFeatureLensPromoBubbleForeground) \
  /* Find bar colors. */ \
  E_CPONLY(kColorFindBarBackground) \
  E_CPONLY(kColorFindBarButtonIcon) \
  E_CPONLY(kColorFindBarButtonIconDisabled) \
  E_CPONLY(kColorFindBarForeground) \
  E_CPONLY(kColorFindBarMatchCount) \
  /* Flying Indicator colors. */ \
  E_CPONLY(kColorFlyingIndicatorBackground) \
  E_CPONLY(kColorFlyingIndicatorForeground) \
  /* Default accessibility focus highlight. */ \
  E_CPONLY(kColorFocusHighlightDefault) \
  /* Frame caption colors. */ \
  E_CPONLY(kColorFrameCaptionActive) \
  E_CPONLY(kColorFrameCaptionInactive) \
  /* History embedding colors. */ \
  E_CPONLY(kColorHistoryEmbeddingsBackground) \
  E_CPONLY(kColorHistoryEmbeddingsDivider) \
  E_CPONLY(kColorHistoryEmbeddingsForeground) \
  E_CPONLY(kColorHistoryEmbeddingsForegroundSubtle) \
  E_CPONLY(kColorHistoryEmbeddingsImageBackground) \
  E_CPONLY(kColorHistoryEmbeddingsImageBackgroundGradientEnd) \
  E_CPONLY(kColorHistoryEmbeddingsImageBackgroundGradientStart) \
  E_CPONLY(kColorHistoryEmbeddingsWithAnswersBackground) \
  /* InfoBar colors. */ \
  E_CPONLY(kColorInfoBarBackground) \
  E_CPONLY(kColorInfoBarButtonIcon) \
  E_CPONLY(kColorInfoBarButtonIconDisabled) \
  E_CPONLY(kColorInfoBarContentAreaSeparator) \
  E_CPONLY(kColorInfoBarForeground) \
  /* There is also a kColorInfoBarIcon in /ui/color/color_id.h */ \
  /* Intent Picker colors. */ \
  E_CPONLY(kColorIntentPickerItemBackgroundHovered) \
  E_CPONLY(kColorIntentPickerItemBackgroundSelected) \
  /* Hover Button colors */ \
  E_CPONLY(kColorHoverButtonBackgroundHovered) \
  /* Lens overlay colors. */ \
  E_CPONLY(kColorLensOverlayToastBackground) \
  E_CPONLY(kColorLensOverlayToastButtonBorder) \
  E_CPONLY(kColorLensOverlayToastForeground) \
  /* Location bar colors. */ \
  E_CPONLY(kColorLocationBarBackground) \
  E_CPONLY(kColorLocationBarBackgroundHovered) \
  E_CPONLY(kColorLocationBarBorder) \
  E_CPONLY(kColorLocationBarBorderOnMismatch) \
  E_CPONLY(kColorLocationBarBorderOpaque) \
  E_CPONLY(kColorLocationBarClearAllButtonIcon) \
  E_CPONLY(kColorLocationBarClearAllButtonIconDisabled) \
  /* Media router colors. */ \
  E_CPONLY(kColorMediaRouterIconActive) \
  E_CPONLY(kColorMediaRouterIconWarning) \
  /* New tab button colors. */ \
  E_CPONLY(kColorNewTabButtonForegroundFrameActive) \
  E_CPONLY(kColorNewTabButtonForegroundFrameInactive) \
  E_CPONLY(kColorNewTabButtonBackgroundFrameActive) \
  E_CPONLY(kColorNewTabButtonBackgroundFrameInactive) \
  E_CPONLY(kColorNewTabButtonFocusRing) \
  E_CPONLY(kColorNewTabButtonInkDropFrameActive) \
  E_CPONLY(kColorNewTabButtonInkDropFrameInactive) \
  E_CPONLY(kColorTabStripControlButtonInkDrop) \
  E_CPONLY(kColorTabStripControlButtonInkDropRipple) \
  /* New tab button colors for ChromeRefresh.*/ \
  /* TODO (crbug.com/1399942) remove when theming works */ \
  E_CPONLY(kColorNewTabButtonCRForegroundFrameActive) \
  E_CPONLY(kColorNewTabButtonCRForegroundFrameInactive) \
  E_CPONLY(kColorNewTabButtonCRBackgroundFrameActive) \
  E_CPONLY(kColorNewTabButtonCRBackgroundFrameInactive) \
  /* New Tab Page colors. */ \
  E_CPONLY(kColorNewTabPageActionButtonBackground) \
  E_CPONLY(kColorNewTabPageActionButtonBorder) \
  E_CPONLY(kColorNewTabPageActionButtonBorderHovered) \
  E_CPONLY(kColorNewTabPageActionButtonForeground) \
  E_CPONLY(kColorNewTabPageActiveBackground) \
  E_CPONLY(kColorNewTabPageAddShortcutBackground) \
  E_CPONLY(kColorNewTabPageAddShortcutForeground) \
  E_CPONLY(kColorNewTabPageAttributionForeground) \
  E_CPONLY(kColorNewTabPageBackground) \
  E_CPONLY(kColorNewTabPageBackgroundOverride) \
  E_CPONLY(kColorNewTabPageBorder) \
  E_CPONLY(kColorNewTabPageButtonBackground) \
  E_CPONLY(kColorNewTabPageButtonBackgroundHovered) \
  E_CPONLY(kColorNewTabPageButtonForeground) \
  E_CPONLY(kColorNewTabPageCartModuleDiscountChipBackground) \
  E_CPONLY(kColorNewTabPageCartModuleDiscountChipForeground) \
  E_CPONLY(kColorNewTabPageChipBackground) \
  E_CPONLY(kColorNewTabPageChipForeground) \
  E_CPONLY(kColorNewTabPageControlBackgroundHovered) \
  E_CPONLY(kColorNewTabPageControlBackgroundSelected) \
  E_CPONLY(kColorNewTabPageDialogBackground) \
  E_CPONLY(kColorNewTabPageDialogBackgroundActive) \
  E_CPONLY(kColorNewTabPageDialogBorder) \
  E_CPONLY(kColorNewTabPageDialogBorderSelected) \
  E_CPONLY(kColorNewTabPageDialogControlBackgroundHovered) \
  E_CPONLY(kColorNewTabPageDialogForeground) \
  E_CPONLY(kColorNewTabPageDialogSecondaryForeground) \
  E_CPONLY(kColorNewTabPageFirstRunBackground) \
  E_CPONLY(kColorNewTabPageFocusRing) \
  E_CPONLY(kColorNewTabPageHeader) \
  E_CPONLY(kColorNewTabPageHistoryClustersModuleItemBackground) \
  E_CPONLY(kColorNewTabPagePromoBackground) \
  E_CPONLY(kColorNewTabPagePromoImageBackground) \
  E_CPONLY(kColorNewTabPageIconButtonBackground) \
  E_CPONLY(kColorNewTabPageIconButtonBackgroundActive) \
  E_CPONLY(kColorNewTabPageLink) \
  E_CPONLY(kColorNewTabPageLogo) \
  E_CPONLY(kColorNewTabPageLogoUnthemedDark) \
  E_CPONLY(kColorNewTabPageLogoUnthemedLight) \
  E_CPONLY(kColorNewTabPageMenuInnerShadow) \
  E_CPONLY(kColorNewTabPageMenuOuterShadow) \
  E_CPONLY(kColorNewTabPageMicBorderColor) \
  E_CPONLY(kColorNewTabPageMicIconColor) \
  E_CPONLY(kColorNewTabPageMobilePromoDismissButton) \
  E_CPONLY(kColorNewTabPageModuleControlBorder) \
  E_CPONLY(kColorNewTabPageModuleContextMenuDivider) \
  E_CPONLY(kColorNewTabPageModuleBackground) \
  E_CPONLY(kColorNewTabPageModuleCalendarEventTimeStatusBackground) \
  E_CPONLY(kColorNewTabPageModuleCalendarAttachmentScrollbarThumb) \
  E_CPONLY(kColorNewTabPageModuleCalendarDividerColor) \
  E_CPONLY(kColorNewTabPageModuleIconBackground) \
  E_CPONLY(kColorNewTabPageModuleElementDivider) \
  E_CPONLY(kColorNewTabPageModuleIconContainerBackground) \
  E_CPONLY(kColorNewTabPageModuleItemBackground) \
  E_CPONLY(kColorNewTabPageModuleItemBackgroundHovered) \
  E_CPONLY(kColorNewTabPageModuleScrollButtonBackground) \
  E_CPONLY(kColorNewTabPageModuleScrollButtonBackgroundHovered) \
  E_CPONLY(kColorNewTabPageMostVisitedForeground) \
  E_CPONLY(kColorNewTabPageMostVisitedTileBackground) \
  E_CPONLY(kColorNewTabPageMostVisitedTileBackgroundThemed) \
  E_CPONLY(kColorNewTabPageMostVisitedTileBackgroundUnthemed) \
  E_CPONLY(kColorNewTabPageOnThemeForeground) \
  E_CPONLY(kColorNewTabPageOverlayBackground) \
  E_CPONLY(kColorNewTabPageOverlayForeground) \
  E_CPONLY(kColorNewTabPageOverlaySecondaryForeground) \
  E_CPONLY(kColorNewTabPagePrimaryForeground) \
  E_CPONLY(kColorNewTabPageSearchBoxBackground) \
  E_CPONLY(kColorNewTabPageSearchBoxBackgroundHovered) \
  E_CPONLY(kColorNewTabPageSearchBoxResultsTextDimmedSelected) \
  E_CPONLY(kColorNewTabPageSecondaryForeground) \
  E_CPONLY(kColorNewTabPageSectionBorder) \
  E_CPONLY(kColorNewTabPageSelectedBackground) \
  E_CPONLY(kColorNewTabPageSelectedBorder) \
  E_CPONLY(kColorNewTabPageSelectedForeground) \
  E_CPONLY(kColorNewTabPageTagBackground) \
  E_CPONLY(kColorNewTabPageText) \
  E_CPONLY(kColorNewTabPageTextUnthemed) \
  E_CPONLY(kColorNewTabPageTextLight) \
  E_CPONLY(kColorNewTabPageWallpaperSearchButtonBackground) \
  E_CPONLY(kColorNewTabPageWallpaperSearchButtonBackgroundHovered) \
  E_CPONLY(kColorNewTabPageWallpaperSearchButtonForeground) \
  /* New Tab Page Colors for Doodle Share Button. */ \
  E_CPONLY(kColorNewTabPageDoodleShareButtonBackground) \
  E_CPONLY(kColorNewTabPageDoodleShareButtonIcon) \
  /* Omnibox colors. */ \
  E_CPONLY(kColorOmniboxAnswerIconGM3Background) \
  E_CPONLY(kColorOmniboxAnswerIconGM3Foreground) \
  E_CPONLY(kColorOmniboxBubbleOutline) \
  E_CPONLY(kColorOmniboxBubbleOutlineExperimentalKeywordMode) \
  E_CPONLY(kColorOmniboxChipInUseActivityIndicatorBackground) \
  E_CPONLY(kColorOmniboxChipInUseActivityIndicatorForeground) \
  E_CPONLY(kColorOmniboxChipBackground) \
  E_CPONLY(kColorOmniboxChipBlockedActivityIndicatorBackground) \
  E_CPONLY(kColorOmniboxChipBlockedActivityIndicatorForeground) \
  E_CPONLY(kColorOmniboxChipForegroundLowVisibility) \
  E_CPONLY(kColorOmniboxChipForegroundNormalVisibility) \
  E_CPONLY(kColorOmniboxChipInkDropHover) \
  E_CPONLY(kColorOmniboxChipInkDropRipple) \
  E_CPONLY(kColorOmniboxChipOnSystemBlockedActivityIndicatorBackground) \
  E_CPONLY(kColorOmniboxChipOnSystemBlockedActivityIndicatorForeground) \
  E_CPONLY(kColorOmniboxIntentChipBackground) \
  E_CPONLY(kColorOmniboxIntentChipIcon) \
  E_CPONLY(kColorOmniboxKeywordSelected) \
  E_CPONLY(kColorOmniboxKeywordSeparator) \
  E_CPONLY(kColorOmniboxResultsBackground) \
  E_CPONLY(kColorOmniboxResultsBackgroundHovered) \
  E_CPONLY(kColorOmniboxResultsBackgroundSelected) \
  E_CPONLY(kColorOmniboxResultsBackgroundIPH) \
  E_CPONLY(kColorOmniboxResultsButtonBorder) \
  E_CPONLY(kColorOmniboxResultsButtonIcon) \
  E_CPONLY(kColorOmniboxResultsButtonIconSelected) \
  E_CPONLY(kColorOmniboxResultsButtonInkDrop) \
  E_CPONLY(kColorOmniboxResultsButtonInkDropRowHovered) \
  E_CPONLY(kColorOmniboxResultsButtonInkDropRowSelected) \
  E_CPONLY(kColorOmniboxResultsButtonInkDropSelected) \
  E_CPONLY(kColorOmniboxResultsButtonInkDropSelectedRowHovered) \
  E_CPONLY(kColorOmniboxResultsButtonInkDropSelectedRowSelected) \
  E_CPONLY(kColorOmniboxResultsChipBackground) \
  E_CPONLY(kColorOmniboxResultsFocusIndicator) \
  E_CPONLY(kColorOmniboxResultsIcon) \
  E_CPONLY(kColorOmniboxResultsIconGM3Background) \
  E_CPONLY(kColorOmniboxResultsIconSelected) \
  E_CPONLY(kColorOmniboxResultsStarterPackIcon) \
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
  E_CPONLY(kColorOmniboxSecurityChipDangerousBackground) \
  E_CPONLY(kColorOmniboxSecurityChipDefault) \
  E_CPONLY(kColorOmniboxSecurityChipInkDropHover) \
  E_CPONLY(kColorOmniboxSecurityChipInkDropRipple) \
  E_CPONLY(kColorOmniboxSecurityChipSecure) \
  E_CPONLY(kColorOmniboxSecurityChipText) \
  E_CPONLY(kColorOmniboxSelectionBackground) \
  E_CPONLY(kColorOmniboxSelectionForeground) \
  E_CPONLY(kColorOmniboxText) \
  E_CPONLY(kColorOmniboxTextDimmed) \
  /* Page Info colors */ \
  E_CPONLY(kColorPageActionIcon) \
  E_CPONLY(kColorPageActionIconHover) \
  E_CPONLY(kColorPageInfoBackground) \
  E_CPONLY(kColorPageInfoBackgroundTonal) \
  E_CPONLY(kColorPageInfoChosenObjectDeleteButtonIcon) \
  E_CPONLY(kColorPageInfoChosenObjectDeleteButtonIconDisabled) \
  E_CPONLY(kColorPageInfoForeground) \
  E_CPONLY(kColorPageInfoSubtitleForeground) \
  E_CPONLY(kColorPageInfoForegroundTonal) \
  E_CPONLY(kColorPageInfoIconHover) \
  E_CPONLY(kColorPageInfoIconPressed) \
  E_CPONLY(kColorPageInfoPermissionBlockedOnSystemLevelDisabled) \
  E_CPONLY(kColorPageInfoPermissionForeground) \
  E_CPONLY(kColorPageInfoPermissionUsedIcon) \
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
  /* Permission Prompt colors. */ \
  E_CPONLY(kColorPermissionPromptRequestText) \
   /* Performance Intervention colors. */ \
  E_CPONLY(kColorPerformanceInterventionButtonIconActive) \
  E_CPONLY(kColorPerformanceInterventionButtonIconInactive) \
  /* Picture-in-Picture window colors. */ \
  E_CPONLY(kColorPipWindowBackToTabButtonBackground) \
  E_CPONLY(kColorPipWindowBackground) \
  E_CPONLY(kColorPipWindowControlsBackground) \
  E_CPONLY(kColorPipWindowTopBarBackground) \
  E_CPONLY(kColorPipWindowForeground) \
  E_CPONLY(kColorPipWindowForegroundInactive) \
  E_CPONLY(kColorPipWindowHangUpButtonForeground) \
  E_CPONLY(kColorPipWindowSkipAdButtonBackground) \
  E_CPONLY(kColorPipWindowSkipAdButtonBorder) \
  /* Product Specifications colors */ \
  E_CPONLY(kColorProductSpecificationsButtonBackground) \
  E_CPONLY(kColorProductSpecificationsCitationBackground) \
  E_CPONLY(kColorProductSpecificationsDetailChipBackground) \
  E_CPONLY(kColorProductSpecificationsDisclosureBackground) \
  E_CPONLY(kColorProductSpecificationsDisclosureForeground) \
  E_CPONLY(kColorProductSpecificationsDisclosureGradientEnd) \
  E_CPONLY(kColorProductSpecificationsDisclosureGradientStart) \
  E_CPONLY(kColorProductSpecificationsDisclosureSummaryBackground) \
  E_CPONLY(kColorProductSpecificationsDivider) \
  E_CPONLY(kColorProductSpecificationsGradientIcon) \
  E_CPONLY(kColorProductSpecificationsHorizontalCarouselScrollbarThumb) \
  E_CPONLY(kColorProductSpecificationsIcon) \
  E_CPONLY(kColorProductSpecificationsIconButtonBackground) \
  E_CPONLY(kColorProductSpecificationsIconButtonHoveredBackground) \
  E_CPONLY(kColorProductSpecificationsLink) \
  E_CPONLY(kColorProductSpecificationsPageBackground) \
  E_CPONLY(kColorProductSpecificationsPrimaryTitle) \
  E_CPONLY(kColorProductSpecificationsSecondaryTitle) \
  E_CPONLY(kColorProductSpecificationsSummaryBackground) \
  E_CPONLY(kColorProductSpecificationsSummaryBackgroundDragging) \
  E_CPONLY(kColorProductSpecificationsTonalButtonBackground) \
  /* Profile Menu colors. */ \
  E_CPONLY(kColorProfileMenuBackground) \
  E_CPONLY(kColorProfileMenuHeaderBackground) \
  E_CPONLY(kColorProfileMenuIdentityInfoBackground) \
  E_CPONLY(kColorProfileMenuIdentityInfoTitle) \
  E_CPONLY(kColorProfileMenuIdentityInfoSubtitle) \
  E_CPONLY(kColorProfileMenuHeaderLabel) \
  E_CPONLY(kColorProfileMenuIconButton) \
  E_CPONLY(kColorProfileMenuIconButtonBackground) \
  E_CPONLY(kColorProfileMenuIconButtonBackgroundHovered) \
  E_CPONLY(kColorProfileMenuSyncErrorIcon) \
  E_CPONLY(kColorProfileMenuSyncIcon) \
  E_CPONLY(kColorProfileMenuSyncInfoBackground) \
  E_CPONLY(kColorProfileMenuSyncOffIcon) \
  E_CPONLY(kColorProfileMenuSyncPausedIcon) \
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
    /* Read Anything colors. */ \
  E_CPONLY(kColorReadAnythingBackground) \
  E_CPONLY(kColorReadAnythingBackgroundBlue) \
  E_CPONLY(kColorReadAnythingBackgroundDark) \
  E_CPONLY(kColorReadAnythingBackgroundLight) \
  E_CPONLY(kColorReadAnythingBackgroundYellow) \
  E_CPONLY(kColorReadAnythingCurrentReadAloudHighlight) \
  E_CPONLY(kColorReadAnythingCurrentReadAloudHighlightBlue) \
  E_CPONLY(kColorReadAnythingCurrentReadAloudHighlightDark) \
  E_CPONLY(kColorReadAnythingCurrentReadAloudHighlightLight) \
  E_CPONLY(kColorReadAnythingCurrentReadAloudHighlightYellow) \
  E_CPONLY(kColorReadAnythingFocusRingBackground) \
  E_CPONLY(kColorReadAnythingFocusRingBackgroundBlue) \
  E_CPONLY(kColorReadAnythingFocusRingBackgroundDark) \
  E_CPONLY(kColorReadAnythingFocusRingBackgroundLight) \
  E_CPONLY(kColorReadAnythingFocusRingBackgroundYellow) \
  E_CPONLY(kColorReadAnythingForeground) \
  E_CPONLY(kColorReadAnythingForegroundBlue) \
  E_CPONLY(kColorReadAnythingForegroundDark) \
  E_CPONLY(kColorReadAnythingForegroundLight) \
  E_CPONLY(kColorReadAnythingForegroundYellow) \
  E_CPONLY(kColorReadAnythingSeparator) \
  E_CPONLY(kColorReadAnythingSeparatorBlue) \
  E_CPONLY(kColorReadAnythingSeparatorDark) \
  E_CPONLY(kColorReadAnythingSeparatorLight) \
  E_CPONLY(kColorReadAnythingSeparatorYellow) \
  E_CPONLY(kColorReadAnythingDropdownBackground) \
  E_CPONLY(kColorReadAnythingDropdownBackgroundBlue) \
  E_CPONLY(kColorReadAnythingDropdownBackgroundDark) \
  E_CPONLY(kColorReadAnythingDropdownBackgroundLight) \
  E_CPONLY(kColorReadAnythingDropdownBackgroundYellow) \
  E_CPONLY(kColorReadAnythingDropdownSelected) \
  E_CPONLY(kColorReadAnythingDropdownSelectedBlue) \
  E_CPONLY(kColorReadAnythingDropdownSelectedDark) \
  E_CPONLY(kColorReadAnythingDropdownSelectedLight) \
  E_CPONLY(kColorReadAnythingDropdownSelectedYellow) \
  E_CPONLY(kColorReadAnythingTextSelection) \
  E_CPONLY(kColorReadAnythingTextSelectionBlue) \
  E_CPONLY(kColorReadAnythingTextSelectionDark) \
  E_CPONLY(kColorReadAnythingTextSelectionLight) \
  E_CPONLY(kColorReadAnythingTextSelectionYellow) \
  E_CPONLY(kColorReadAnythingLinkDefault) \
  E_CPONLY(kColorReadAnythingLinkDefaultBlue) \
  E_CPONLY(kColorReadAnythingLinkDefaultDark) \
  E_CPONLY(kColorReadAnythingLinkDefaultLight) \
  E_CPONLY(kColorReadAnythingLinkDefaultYellow) \
  E_CPONLY(kColorReadAnythingLinkVisited) \
  E_CPONLY(kColorReadAnythingLinkVisitedBlue) \
  E_CPONLY(kColorReadAnythingLinkVisitedDark) \
  E_CPONLY(kColorReadAnythingLinkVisitedLight) \
  E_CPONLY(kColorReadAnythingLinkVisitedYellow) \
  E_CPONLY(kColorReadAnythingPreviousReadAloudHighlight) \
  E_CPONLY(kColorReadAnythingPreviousReadAloudHighlightBlue) \
  E_CPONLY(kColorReadAnythingPreviousReadAloudHighlightDark) \
  E_CPONLY(kColorReadAnythingPreviousReadAloudHighlightLight) \
  E_CPONLY(kColorReadAnythingPreviousReadAloudHighlightYellow) \
  /* Realbox colors. */ \
  E_CPONLY(kColorSearchboxAnswerIconBackground) \
  E_CPONLY(kColorSearchboxAnswerIconForeground) \
  E_CPONLY(kColorSearchboxBackground) \
  E_CPONLY(kColorSearchboxBackgroundHovered) \
  E_CPONLY(kColorSearchboxBorder) \
  E_CPONLY(kColorSearchboxForeground) \
  E_CPONLY(kColorSearchboxLensVoiceIconBackground) \
  E_CPONLY(kColorSearchboxPlaceholder) \
  E_CPONLY(kColorSearchboxResultsActionChip) \
  E_CPONLY(kColorSearchboxResultsActionChipIcon) \
  E_CPONLY(kColorSearchboxResultsActionChipFocusOutline) \
  E_CPONLY(kColorSearchboxResultsBackground) \
  E_CPONLY(kColorSearchboxResultsBackgroundHovered) \
  E_CPONLY(kColorSearchboxResultsButtonHover) \
  E_CPONLY(kColorSearchboxResultsDimSelected) \
  E_CPONLY(kColorSearchboxResultsFocusIndicator) \
  E_CPONLY(kColorSearchboxResultsForeground) \
  E_CPONLY(kColorSearchboxResultsForegroundDimmed) \
  E_CPONLY(kColorSearchboxResultsIcon) \
  E_CPONLY(kColorSearchboxResultsIconFocusedOutline) \
  E_CPONLY(kColorSearchboxResultsIconSelected) \
  E_CPONLY(kColorSearchboxResultsUrl) \
  E_CPONLY(kColorSearchboxResultsUrlSelected) \
  E_CPONLY(kColorSearchboxSearchIconBackground) \
  E_CPONLY(kColorSearchboxSelectionBackground) \
  E_CPONLY(kColorSearchboxSelectionForeground) \
  E_CPONLY(kColorSearchboxShadow) \
  /* The colors used for saved tab group chips on the bookmark bar. */ \
  E_CPONLY(kColorSavedTabGroupForegroundGrey) \
  E_CPONLY(kColorSavedTabGroupForegroundBlue) \
  E_CPONLY(kColorSavedTabGroupForegroundRed) \
  E_CPONLY(kColorSavedTabGroupForegroundYellow) \
  E_CPONLY(kColorSavedTabGroupForegroundGreen) \
  E_CPONLY(kColorSavedTabGroupForegroundPink) \
  E_CPONLY(kColorSavedTabGroupForegroundPurple) \
  E_CPONLY(kColorSavedTabGroupForegroundCyan) \
  E_CPONLY(kColorSavedTabGroupForegroundOrange) \
  E_CPONLY(kColorSavedTabGroupOutlineGrey) \
  E_CPONLY(kColorSavedTabGroupOutlineBlue) \
  E_CPONLY(kColorSavedTabGroupOutlineRed) \
  E_CPONLY(kColorSavedTabGroupOutlineYellow) \
  E_CPONLY(kColorSavedTabGroupOutlineGreen) \
  E_CPONLY(kColorSavedTabGroupOutlinePink) \
  E_CPONLY(kColorSavedTabGroupOutlinePurple) \
  E_CPONLY(kColorSavedTabGroupOutlineCyan) \
  E_CPONLY(kColorSavedTabGroupOutlineOrange) \
  /* Screenshot captured bubble colors. */ \
  E_CPONLY(kColorScreenshotCapturedImageBackground) \
  E_CPONLY(kColorScreenshotCapturedImageBorder) \
  /* Share-this-tab dialog colors. */ \
  E_CPONLY(kColorShareThisTabAudioToggleBackground) \
  E_CPONLY(kColorShareThisTabSourceViewBorder) \
  /* Experimentation */ \
  E_CPONLY(kColorShoppingPageActionIconBackgroundVariant) \
  E_CPONLY(kColorShoppingPageActionIconForegroundVariant) \
  /* Side panel colors. */ \
  E_CPONLY(kColorSidePanelBackground) \
  E_CPONLY(kColorSidePanelBadgeBackground) \
  E_CPONLY(kColorSidePanelBadgeBackgroundUpdated) \
  E_CPONLY(kColorSidePanelBadgeForeground) \
  E_CPONLY(kColorSidePanelBadgeForegroundUpdated) \
  E_CPONLY(kColorSidePanelBookmarksSelectedFolderBackground) \
  E_CPONLY(kColorSidePanelBookmarksSelectedFolderForeground) \
  E_CPONLY(kColorSidePanelBookmarksSelectedFolderIcon) \
  E_CPONLY(kColorSidePanelCardBackground) \
  E_CPONLY(kColorSidePanelCardPrimaryForeground) \
  E_CPONLY(kColorSidePanelCardSecondaryForeground) \
  E_CPONLY(kColorSidePanelCommerceGraphAxis) \
  E_CPONLY(kColorSidePanelCommerceGraphBubbleBackground) \
  E_CPONLY(kColorSidePanelCommerceGraphLine) \
  E_CPONLY(kColorSidePanelContentAreaSeparator) \
  E_CPONLY(kColorSidePanelContentBackground) \
  E_CPONLY(kColorSidePanelCustomizeChromeClassicChromeTileBorder) \
  E_CPONLY(kColorSidePanelCustomizeChromeCornerNtpBorder) \
  E_CPONLY(kColorSidePanelCustomizeChromeCustomOptionBackground) \
  E_CPONLY(kColorSidePanelCustomizeChromeCustomOptionForeground) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpActiveTab) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpArrowsAndRefreshButton) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpBackground) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpBorder) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpCaron) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpCaronContainer) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpChromeLogo) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpOmnibox) \
  E_CPONLY(kColorSidePanelCustomizeChromeMiniNtpTabStripBackground) \
  E_CPONLY(kColorSidePanelCustomizeChromeThemeBackground) \
  E_CPONLY(kColorSidePanelCustomizeChromeThemeCheckmarkBackground) \
  E_CPONLY(kColorSidePanelCustomizeChromeThemeCheckmarkForeground) \
  E_CPONLY(kColorSidePanelCustomizeChromeThemeSnapshotBackground) \
  E_CPONLY(kColorSidePanelCustomizeChromeWebStoreBorder) \
  E_CPONLY(kColorSidePanelDialogBackground) \
  E_CPONLY(kColorSidePanelDialogDivider) \
  E_CPONLY(kColorSidePanelDialogPrimaryForeground) \
  E_CPONLY(kColorSidePanelDialogSecondaryForeground) \
  E_CPONLY(kColorSidePanelDivider) \
  E_CPONLY(kColorSidePanelEditFooterBorder) \
  E_CPONLY(kColorSidePanelComboboxEntryIcon) \
  E_CPONLY(kColorSidePanelComboboxEntryTitle) \
  E_CPONLY(kColorSidePanelEntryIcon) \
  E_CPONLY(kColorSidePanelEntryDropdownIcon) \
  E_CPONLY(kColorSidePanelEntryTitle) \
  E_CPONLY(kColorSidePanelFilterChipBorder) \
  E_CPONLY(kColorSidePanelFilterChipForeground) \
  E_CPONLY(kColorSidePanelFilterChipForegroundSelected) \
  E_CPONLY(kColorSidePanelFilterChipIcon) \
  E_CPONLY(kColorSidePanelFilterChipIconSelected) \
  E_CPONLY(kColorSidePanelFilterChipBackgroundHover) \
  E_CPONLY(kColorSidePanelFilterChipBackgroundSelected) \
  E_CPONLY(kColorSidePanelHeaderButtonIcon) \
  E_CPONLY(kColorSidePanelHeaderButtonIconDisabled) \
  E_CPONLY(kColorSidePanelHoverResizeAreaHandle) \
  E_CPONLY(kColorSidePanelResizeAreaHandle) \
  E_CPONLY(kColorSidePanelScrollbarThumb) \
  E_CPONLY(kColorSidePanelTextfieldBorder) \
  E_CPONLY(kColorSidePanelWallpaperSearchTileBackground) \
  E_CPONLY(kColorSidePanelWallpaperSearchErrorButtonBackground) \
  E_CPONLY(kColorSidePanelWallpaperSearchErrorButtonText) \
  E_CPONLY(kColorSidePanelWallpaperSearchInspirationDescriptors) \
  /* Status bubble colors. */ \
  E_CPONLY(kColorStatusBubbleBackgroundFrameActive) \
  E_CPONLY(kColorStatusBubbleBackgroundFrameInactive) \
  E_CPONLY(kColorStatusBubbleForegroundFrameActive) \
  E_CPONLY(kColorStatusBubbleForegroundFrameInactive) \
  E_CPONLY(kColorStatusBubbleShadow) \
  /* Tab alert colors in tab strip. */ \
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
  /* Tab alert colors in hover cards */ \
  E_CPONLY(kColorHoverCardTabAlertMediaRecordingIcon) \
  E_CPONLY(kColorHoverCardTabAlertPipPlayingIcon) \
  E_CPONLY(kColorHoverCardTabAlertAudioPlayingIcon) \
  /* Tab colors. */ \
  E_CPONLY(kColorTabBackgroundActiveFrameActive) \
  E_CPONLY(kColorTabBackgroundActiveFrameInactive) \
  E_CPONLY(kColorTabBackgroundInactiveFrameActive) \
  E_CPONLY(kColorTabBackgroundInactiveFrameInactive) \
  E_CPONLY(kColorTabBackgroundInactiveHoverFrameActive) \
  E_CPONLY(kColorTabBackgroundInactiveHoverFrameInactive) \
  E_CPONLY(kColorTabBackgroundSelectedFrameActive) \
  E_CPONLY(kColorTabBackgroundSelectedFrameInactive) \
  E_CPONLY(kColorTabBackgroundSelectedHoverFrameActive) \
  E_CPONLY(kColorTabBackgroundSelectedHoverFrameInactive) \
  E_CPONLY(kColorTabCloseButtonFocusRingActive) \
  E_CPONLY(kColorTabCloseButtonFocusRingInactive) \
  E_CPONLY(kColorTabDiscardRingFrameActive) \
  E_CPONLY(kColorTabDiscardRingFrameInactive) \
  E_CPONLY(kColorTabFocusRingActive) \
  E_CPONLY(kColorTabFocusRingInactive) \
  E_CPONLY(kColorTabForegroundActiveFrameActive) \
  E_CPONLY(kColorTabForegroundActiveFrameInactive) \
  E_CPONLY(kColorTabForegroundInactiveFrameActive) \
  E_CPONLY(kColorTabForegroundInactiveFrameInactive) \
  E_CPONLY(kColorTabDividerFrameActive) \
  E_CPONLY(kColorTabDividerFrameInactive) \
  E_CPONLY(kColorTabHoverCardBackground) \
  E_CPONLY(kColorTabHoverCardForeground) \
  E_CPONLY(kColorTabHoverCardSecondaryText) \
  /* Tab group bookmark bar colors. */ \
  E_CPONLY(kColorTabGroupBookmarkBarGrey) \
  E_CPONLY(kColorTabGroupBookmarkBarBlue) \
  E_CPONLY(kColorTabGroupBookmarkBarRed) \
  E_CPONLY(kColorTabGroupBookmarkBarYellow) \
  E_CPONLY(kColorTabGroupBookmarkBarGreen) \
  E_CPONLY(kColorTabGroupBookmarkBarPink) \
  E_CPONLY(kColorTabGroupBookmarkBarPurple) \
  E_CPONLY(kColorTabGroupBookmarkBarCyan) \
  E_CPONLY(kColorTabGroupBookmarkBarOrange) \
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
  E_CPONLY(kColorTabGroupDialogIconEnabled) \
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
  E_CPONLY(kColorTabStrokeFrameActive) \
  E_CPONLY(kColorTabStrokeFrameInactive) \
  E_CPONLY(kColorTabstripLoadingProgressBackground) \
  E_CPONLY(kColorTabstripLoadingProgressForeground) \
  E_CPONLY(kColorTabstripScrollContainerShadow) \
  E_CPONLY(kColorTabThrobber) \
  E_CPONLY(kColorTabThrobberPreconnect) \
  /* Tab Search colors */ \
  E_CPONLY(kColorTabSearchButtonBackground) \
  E_CPONLY(kColorTabSearchBackground) \
  E_CPONLY(kColorTabSearchButtonCRForegroundFrameActive) \
  E_CPONLY(kColorTabSearchButtonCRForegroundFrameInactive) \
  E_CPONLY(kColorTabSearchCardBackground) \
  E_CPONLY(kColorTabSearchDivider) \
  E_CPONLY(kColorTabSearchFooterBackground) \
  E_CPONLY(kColorTabSearchImageTabContentBottom) \
  E_CPONLY(kColorTabSearchImageTabContentTop) \
  E_CPONLY(kColorTabSearchImageTabText) \
  E_CPONLY(kColorTabSearchImageWindowFrame) \
  E_CPONLY(kColorTabSearchMediaIcon) \
  E_CPONLY(kColorTabSearchMediaRecordingIcon) \
  E_CPONLY(kColorTabSearchPrimaryForeground) \
  E_CPONLY(kColorTabSearchSecondaryForeground) \
  E_CPONLY(kColorTabSearchSelected) \
  E_CPONLY(kColorTabSearchScrollbarThumb) \
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
  E_CPONLY(kColorToolbar) \
  E_CPONLY(kColorToolbarBackgroundSubtleEmphasis) \
  E_CPONLY(kColorToolbarBackgroundSubtleEmphasisHovered) \
  E_CPONLY(kColorToolbarButtonBackgroundHighlightedDefault) \
  E_CPONLY(kColorToolbarButtonBorder) \
  E_CPONLY(kColorToolbarButtonIcon) \
  E_CPONLY(kColorToolbarButtonIconDefault) \
  E_CPONLY(kColorToolbarButtonIconDisabled) \
  E_CPONLY(kColorToolbarButtonIconHovered) \
  E_CPONLY(kColorToolbarButtonIconInactive) \
  E_CPONLY(kColorToolbarButtonIconPressed) \
  E_CPONLY(kColorToolbarButtonText) \
  E_CPONLY(kColorToolbarContentAreaSeparator) \
  E_CPONLY(kColorToolbarExtensionSeparatorDisabled) \
  E_CPONLY(kColorToolbarExtensionSeparatorEnabled) \
  E_CPONLY(kColorToolbarFeaturePromoHighlight) \
  E_CPONLY(kColorToolbarIconContainerBorder) \
  E_CPONLY(kColorToolbarInkDrop) \
  E_CPONLY(kColorToolbarInkDropHover) \
  E_CPONLY(kColorToolbarInkDropRipple) \
  E_CPONLY(kColorToolbarSeparator) \
  E_CPONLY(kColorToolbarActionItemEngaged) \
  E_CPONLY(kColorToolbarSeparatorDefault) \
  E_CPONLY(kColorToolbarText) \
  E_CPONLY(kColorToolbarTextDefault) \
  E_CPONLY(kColorToolbarTextDisabled) \
  E_CPONLY(kColorToolbarTextDisabledDefault) \
  E_CPONLY(kColorToolbarTopSeparatorFrameActive) \
  E_CPONLY(kColorToolbarTopSeparatorFrameInactive) \
  /* WebAuthn colors. */ \
  E_CPONLY(kColorWebAuthnHoverButtonForeground) \
  E_CPONLY(kColorWebAuthnHoverButtonForegroundDisabled) \
  E_CPONLY(kColorWebAuthnBackArrowButtonIcon) \
  E_CPONLY(kColorWebAuthnBackArrowButtonIconDisabled) \
  E_CPONLY(kColorWebAuthnIconColor) \
  E_CPONLY(kColorWebAuthnIconColorDisabled) \
  E_CPONLY(kColorWebAuthnPinTextfieldBottomBorder) \
  E_CPONLY(kColorWebAuthnProgressRingBackground) \
  E_CPONLY(kColorWebAuthnProgressRingForeground) \
  /* Web contents colors. */ \
  E_CPONLY(kColorWebContentsBackground) \
  E_CPONLY(kColorWebContentsBackgroundLetterboxing) \
  /* WebUI Tab Strip colors. */ \
  E_CPONLY(kColorWebUiTabStripBackground) \
  E_CPONLY(kColorWebUiTabStripFocusOutline) \
  E_CPONLY(kColorWebUiTabStripIndicatorCapturing) \
  E_CPONLY(kColorWebUiTabStripIndicatorPip) \
  E_CPONLY(kColorWebUiTabStripIndicatorRecording) \
  E_CPONLY(kColorWebUiTabStripScrollbarThumb) \
  E_CPONLY(kColorWebUiTabStripTabActiveTitleBackground) \
  E_CPONLY(kColorWebUiTabStripTabActiveTitleContent) \
  E_CPONLY(kColorWebUiTabStripTabBackground) \
  E_CPONLY(kColorWebUiTabStripTabBlocked) \
  E_CPONLY(kColorWebUiTabStripTabLoadingSpinning) \
  E_CPONLY(kColorWebUiTabStripTabSeparator) \
  E_CPONLY(kColorWebUiTabStripTabText) \
  E_CPONLY(kColorWebUiTabStripTabWaitingSpinning) \
  /* Window control button background colors. */ \
  E_CPONLY(kColorWindowControlButtonBackgroundActive) \
  E_CPONLY(kColorWindowControlButtonBackgroundInactive) \

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
    E_CPONLY(kColorAccentBorderActive) \
    E_CPONLY(kColorAccentBorderInactive) \
    /* Caption colors. */ \
    E_CPONLY(kColorCaptionButtonForegroundActive) \
    E_CPONLY(kColorCaptionButtonForegroundInactive) \
    E_CPONLY(kColorCaptionCloseButtonBackgroundHovered) \
    E_CPONLY(kColorCaptionCloseButtonForegroundHovered) \
    E_CPONLY(kColorCaptionForegroundActive) \
    E_CPONLY(kColorCaptionForegroundInactive) \
    /* Tab search caption button colors. */ \
    E_CPONLY(kColorTabSearchCaptionButtonFocusRing)
#else
#define CHROME_PLATFORM_SPECIFIC_COLOR_IDS
#endif  // BUILDFLAG(IS_WIN)

#define CHROME_COLOR_IDS \
    COMMON_CHROME_COLOR_IDS CHROME_PLATFORM_SPECIFIC_COLOR_IDS

#include "ui/color/color_id_macros.inc"

enum ChromeColorIds : ui::ColorId {
  kChromeColorsStart = color::kComponentsColorsEnd,

  CHROME_COLOR_IDS

  kChromeColorsEnd,
};

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_macros.inc"

// clang-format on

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
