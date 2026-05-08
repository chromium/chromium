// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_list.h"

#include <vector>

#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

namespace {
// Whenever a feature is added to |kAllFeatures|, it should also be added as
// DEFINE_VARIATION_PARAM in the header, and also added to the
// |kIPHDemoModeChoiceVariations| array.
const base::Feature* const kAllFeatures[] = {
    &kIPHDummyFeature,  // Ensures non-empty array for all platforms.
#if BUILDFLAG(IS_ANDROID)
    // keep-sorted start case=no
    // ALL_FEATURES_ANDROID_START
    &kIPHAccountSettingsHistorySync,
    &kIPHAdaptiveButtonInTopToolbarCustomizationAddToBookmarksFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationGlicFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationOpenInBrowserFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryPdfFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryWebFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationReadAloudFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationTranslateFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature,
    &kIPHAndroidTabDeclutter,
    &kIPHAppRatingPromptFeature,
    &kIPHAppSpecificHistory,
    &kIPHAutoDarkOptOutFeature,
    &kIPHAutoDarkUserEducationMessageFeature,
    &kIPHAutoDarkUserEducationMessageOptInFeature,
    &kIPHBookmarksBarFeature,
    &kIPHCCTHistory,
    &kIPHCCTMinimized,
    &kIPHChromeHomeExpandFeature,
    &kIPHChromeHomePullToRefreshFeature,
    &kIPHChromeReengagementNotification1Feature,
    &kIPHChromeReengagementNotification2Feature,
    &kIPHChromeReengagementNotification3Feature,
    &kIPHContextualPageActionsActionChipFeature,
    &kIPHContextualPageActionsQuietVariantFeature,
    &kIPHDataSaverDetailFeature,
    &kIPHDataSaverMilestonePromoFeature,
    &kIPHDataSaverPreviewFeature,
    &kIPHDefaultBrowserPromoMagicStackFeature,
    &kIPHDefaultBrowserPromoMessagesFeature,
    &kIPHDefaultBrowserPromoSettingCardFeature,
    &kIPHDownloadHomeFeature,
    &kIPHDownloadIndicatorFeature,
    &kIPHDownloadInfoBarDownloadContinuingFeature,
    &kIPHDownloadInfoBarDownloadsAreFasterFeature,
    &kIPHDownloadPageFeature,
    &kIPHDownloadPageScreenshotFeature,
    &kIPHDownloadSettingsFeature,
    &kIPHEphemeralTabFeature,
    &kIPHExploreSitesTileFeature,
    &kIPHExtensionsManageAppMenuFeature,
    &kIPHExtensionsManageToolbarFeature,
    &kIPHFeedCardMenuFeature,
    &kIPHFeedHeaderMenuFeature,
    &kIPHFeedSwipeRefresh,
    &kIPHFuseboxAttachmentFeature,
    &kIPHGenericAlwaysTriggerHelpUiFeature,
    &kIPHGestureUserEducation,
    &kIPHGlicPromoAndroidFeature,
    &kIPHIdentityDiscFeature,
    &kIPHInstanceSwitcherFeature,
    &kIPHKeyboardAccessoryAddressFillingFeature,
    &kIPHKeyboardAccessoryBarSwipingFeature,
    &kIPHKeyboardAccessoryPasswordFillingFeature,
    &kIPHKeyboardAccessoryPaymentFillingFeature,
    &kIPHKeyboardAccessoryPaymentOfferFeature,
    &kIPHLowUserEngagementDetectorFeature,
    &kIPHMenuAddToGroup,
    &kIPHMicToolbarFeature,
    &kIPHMostVisitedTilesCustomizationPinFeature,
    &kIPHNewTabPageThemeCustomizationFeature,
    &kIPHPageInfoFeature,
    &kIPHPageInfoStoreInfoFeature,
    &kIPHPageSummaryPdfMenuFeature,
    &kIPHPageSummaryWebMenuFeature,
    &kIPHPageZoomFeature,
    &kIPHPdfPageDownloadFeature,
    &kIPHPreviewsOmniboxUIFeature,
    &kIPHReadAloudAppMenuFeature,
    &kIPHReadAloudExpandedPlayerFeature,
    &kIPHReadAloudPlaybackModeFeature,
    &kIPHReaderModeDistillInAppFeature,
    &kIPHReadLaterAppMenuBookmarksFeature,
    &kIPHReadLaterAppMenuBookmarkThisPageFeature,
    &kIPHReadLaterBottomSheetFeature,
    &kIPHReadLaterContextMenuFeature,
    &kIPHRequestDesktopSiteDefaultOnFeature,
    &kIPHRequestDesktopSiteExceptionsGenericFeature,
    &kIPHRequestDesktopSiteWindowSettingFeature,
    &kIPHRestoreTabsOnFREFeature,
    &kIPHSharedHighlightingBuilder,
    &kIPHSharedHighlightingReceiverFeature,
    &kIPHShareScreenshotFeature,
    &kIPHSharingHubLinkToggleFeature,
    &kIPHSharingHubWebnotesStylizeFeature,
    &kIPHShoppingListMenuItemFeature,
    &kIPHShoppingListSaveFlowFeature,
    &kIPHSiteControlsFeature,
    &kIPHTabGroupCreationDialogSyncTextFeature,
    &kIPHTabGroupsDragAndDropFeature,
    &kIPHTabGroupShareNoticeFeature,
    &kIPHTabGroupShareNotificationBubbleOnStripFeature,
    &kIPHTabGroupShareUpdateFeature,
    &kIPHTabGroupShareVersionUpdateFeature,
    &kIPHTabGroupsRemoteGroupFeature,
    &kIPHTabGroupsSurfaceFeature,
    &kIPHTabGroupsSurfaceOnHideFeature,
    &kIPHTabGroupSyncOnStripFeature,
    &kIPHTabSwitcherAddToGroup,
    &kIPHTabSwitcherButtonFeature,
    &kIPHTabSwitcherButtonSwitchIncognitoFeature,
    &kIPHTabSwitcherXR,
    &kIPHTabTearingXR,
    &kIPHThreeDotMenuBackButton,
    &kIPHTouchToSearchCalloutFeature,
    &kIPHTranslateMenuButtonFeature,
    &kIPHVideoTutorialNTPChromeIntroFeature,
    &kIPHVideoTutorialNTPDownloadFeature,
    &kIPHVideoTutorialNTPSearchFeature,
    &kIPHVideoTutorialNTPSummaryFeature,
    &kIPHVideoTutorialNTPVoiceSearchFeature,
    &kIPHVideoTutorialTryNowFeature,
// ALL_FEATURES_ANDROID_END
// keep-sorted end
#else
    // keep-sorted start case=no
    &kIPHiOSAddressPromoDesktopFeature,
    &kIPHiOSEnhancedBrowsingDesktopFeature,
    &kIPHiOSLensPromoDesktopFeature,
    &kIPHiOSPasswordPromoDesktopFeature,
    &kIPHiOSPaymentPromoDesktopFeature,
    &kIPHiOSPriceTrackingDesktopFeature,
    &kIPHiOSTabGroupsDesktopFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
    &kIPHBottomToolbarTipFeature,
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
    // keep-sorted start case=no
    &kIPHBadgedReaderModeFeature,
    &kIPHBadgedReadingListFeature,
    &kIPHBadgedTranslateManualTriggerFeature,
    &kIPHDefaultSiteViewFeature,
    &kIPHDiscoverFeedHeaderFeature,
    &kIPHFollowWhileBrowsingFeature,
    &kIPHHomeCustomizationMenuFeature,
    &kIPHiOSActiveDaysTrackingFeature,
    &kIPHiOSAIHubNewBadge,
    &kIPHiOSContextualPanelPriceInsightsFeature,
    &kIPHiOSContextualPanelSampleModelFeature,
    &kIPHiOSDefaultBrowserBadgeEligibilityFeature,
    &kIPHiOSDefaultBrowserBannerPromoFeature,
    &kIPHiOSDefaultBrowserOffCyclePromoFeature,
    &kIPHiOSDefaultBrowserOverflowMenuBadgeFeature,
    &kIPHiOSDockingPromoEligibilityFeature,
    &kIPHiOSDockingPromoFeature,
    &kIPHiOSDownloadAutoDeletionFeature,
    &kIPHiOSFeedSwipeAnimatedFeature,
    &kIPHiOSFeedSwipeStaticFeature,
    &kIPHiOSGeminiContextualCueChip,
    &kIPHiOSGeminiExternalAppStoreEvent,
    &kIPHiOSGeminiFullscreenPromoFeature,
    &kIPHiOSGeminiImageRemixFeature,
    &kIPHiOSHistoryOnOverflowMenuFeature,
    &kIPHiOSHomepageCustomizationNewBadge,
    &kIPHiOSHomepageLensNewBadge,
    &kIPHiOSInlineEnhancedSafeBrowsingPromoFeature,
    &kIPHiOSLensKeyboardFeature,
    &kIPHiOSLensOverlayEntrypointTipFeature,
    &kIPHiOSLensOverlayEscapeHatchTipFeature,
    &kIPHiOSNewIAPromoFeature,
    &kIPHiOSOneTimeDefaultBrowserNotificationFeature,
    &kIPHiOSOverflowMenuCustomizationFeature,
    &kIPHIOSPageActionMenu,
    &kIPHiOSPageInfoRevampFeature,
    &kIPHiOSPinMostVisitedSiteFeature,
    &kIPHiOSPostDefaultAbandonmentPromoFeature,
    &kIPHiOSPromoAllTabsFeature,
    &kIPHiOSPromoAppStoreFeature,
    &kIPHiOSPromoBackgroundCustomizationFeature,
    &kIPHiOSPromoCredentialProviderExtensionFeature,
    &kIPHiOSPromoDefaultBrowserReminderFeature,
    &kIPHiOSPromoGenericDefaultBrowserFeature,
    &kIPHiOSPromoMadeForIOSFeature,
    &kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature,
    &kIPHiOSPromoNonModalShareDefaultBrowserFeature,
    &kIPHiOSPromoNonModalSigninBookmarkFeature,
    &kIPHiOSPromoNonModalSigninPasswordFeature,
    &kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature,
    &kIPHiOSPromoPasswordManagerWidgetFeature,
    &kIPHiOSPromoPostRestoreDefaultBrowserFeature,
    &kIPHiOSPromoPostRestoreFeature,
    &kIPHiOSPromoSigninFullscreenFeature,
    &kIPHiOSPromoStaySafeFeature,
    &kIPHiOSPromoWhatsNewFeature,
    &kIPHiOSPullToRefreshFeature,
    &kIPHiOSReaderModeLargeOmniboxEntrypointFeature,
    &kIPHiOSReaderModeOptionsFeature,
    &kIPHiOSReminderNotificationsOverflowMenuBubbleFeature,
    &kIPHiOSReminderNotificationsOverflowMenuNewBadgeFeature,
    &kIPHiOSReplaceSyncPromosWithSignInPromos,
    &kIPHiOSSafariImportFeature,
    &kIPHiOSSavedTabGroupClosed,
    &kIPHiOSSettingsInOverflowMenuBubbleFeature,
    &kIPHiOSSharedTabGroupForeground,
    &kIPHiOSSwipeBackForwardFeature,
    &kIPHiOSSwipeToolbarToChangeTabFeature,
    &kIPHiOSSwitchAccountsWithNTPAccountParticleDiscFeature,
    &kIPHiOSTabGridSwipeRightForIncognito,
    &kIPHiOSWelcomeBackFeature,
    &kIPHLongPressToolbarTipFeature,
    &kIPHPriceNotificationsWhileBrowsingFeature,
    &kIPHReadingListMessagesFeature,
    &kIPHWhatsNewFeature,
    &kIPHWhatsNewUpdatedFeature,
// keep-sorted end
#else
    &kIPHResumptionRailFeature,
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    &kEsbDownloadRowPromoFeature,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    &kIPHExtensionsMenuFeature,
    &kIPHExtensionsRequestAccessButtonFeature,
    &kIPHExtensionsZeroStatePromoFeature,
#endif
    // keep-sorted start case=no
    &kIPHBackNavigationMenuFeature,
    &kIPHBatterySaverModeFeature,
    &kIPHCompanionSidePanelFeature,
    &kIPHCompanionSidePanelRegionSearchFeature,
    &kIPHComposeMSBBSettingsFeature,
    &kIPHComposeNewBadgeFeature,
    &kIPHDesktopCustomizeChromeAutoOpenFeature,
    &kIPHDesktopCustomizeChromeExperimentFeature,
    &kIPHDesktopPwaInstallFeature,
    &kIPHDesktopRealboxContextualSearchFeature,
    &kIPHDesktopSharedHighlightingFeature,
    &kIPHDiscardRingFeature,
    &kIPHDownloadEsbPromoFeature,
    &kIPHExplicitBrowserSigninPreferenceRememberedFeature,
    &kIPHFocusHelpBubbleScreenReaderPromoFeature,
    &kIPHGlicPromoFeature,
    &kIPHGlicTrustFirstOnboardingShortcutSnoozePromoFeature,
    &kIPHGlicTryItFeature,
    &kIPHGMCCastStartStopFeature,
    &kIPHGMCLocalMediaCastingFeature,
    &kIPHHistorySearchFeature,
    &kIPHLensOverlayFeature,
    &kIPHLensOverlayTranslateButtonFeature,
    &kIPHLiveCaptionFeature,
    &kIPHMemorySaverModeFeature,
    &kIPHMerchantTrustFeature,
    &kIPHPasswordManagerShortcutFeature,
    &kIPHPasswordSharingFeature,
    &kIPHPasswordsManagementBubbleAfterSaveFeature,
    &kIPHPasswordsManagementBubbleDuringSigninFeature,
    &kIPHPasswordsSavePrimingPromoFeature,
    &kIPHPasswordsSaveRecoveryPromoFeature,
    &kIPHPasswordsWebAppProfileSwitchFeature,
    &kIPHPdfInkSignaturesFeature,
    &kIPHPdfSearchifyFeature,
    &kIPHPerformanceInterventionDialogFeature,
    &kIPHPowerBookmarksSidePanelFeature,
    &kIPHPriceInsightsPageActionIconLabelFeature,
    &kIPHPriceTrackingEmailConsentFeature,
    &kIPHPriceTrackingInSidePanelFeature,
    &kIPHPriceTrackingPageActionIconLabelFeature,
    &kIPHProfileSwitchFeature,
    &kIPHPwaQuietNotificationFeature,
    &kIPHReadingListDiscoveryFeature,
    &kIPHReadingListEntryPointFeature,
    &kIPHReadingListInSidePanelFeature,
    &kIPHReadingModeKeyboardShortcutFeature,
    &kIPHReadingModePageActionLabelFeature,
    &kIPHReadingModeSidePanelFeature,
    &kIPHShoppingCollectionFeature,
    &kIPHSideBySidePinnableFeature,
    &kIPHSideBySideTabSwitchFeature,
    &kIPHSidePanelGenericPinnableFeature,
    &kIPHSidePanelLensOverlayPinnableFeature,
    &kIPHSidePanelLensOverlayPinnableFollowupFeature,
    &kIPHSideSearchAutoTriggeringFeature,
    &kIPHSideSearchPageActionLabelFeature,
    &kIPHSmartTabSharingFeature,
    &kIPHTabAudioMutingFeature,
    &kIPHTabGroupsSaveV2CloseGroupFeature,
    &kIPHTabGroupsSaveV2IntroFeature,
    &kIPHTabGroupsSharedTabChangedFeature,
    &kIPHTabGroupsSharedTabFeedbackFeature,
    &kIPHTabSearchComboButtonFeature,
    &kIPHTabSearchToolbarButtonFeature,
    &kIPHVerticalTabsExpandOnHoverFeature,
    &kIPHVerticalTabstripTutorialFeature,
    &kIPHWebUiHelpBubbleTestFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
    // keep-sorted start case=no
    &kIPHAutofillAccountNameEmailSuggestionFeature,
    &kIPHAutofillAiOptInFeature,
    &kIPHAutofillAiValuablesFeature,
    &kIPHAutofillAtMemoryFeature,
    &kIPHAutofillBnplAffirmOrZipSuggestionFeature,
    &kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature,
    &kIPHAutofillCardInfoRetrievalSuggestionFeature,
    &kIPHAutofillCreditCardBenefitFeature,
    &kIPHAutofillDisabledVirtualCardSuggestionFeature,
    &kIPHAutofillDownstreamCardAwarenessFeature,
    &kIPHAutofillEnableLoyaltyCardsFeature,
    &kIPHAutofillExternalAccountProfileSuggestionFeature,
    &kIPHAutofillHomeWorkProfileSuggestionFeature,
    &kIPHAutofillVirtualCardCVCSuggestionFeature,
    &kIPHAutofillVirtualCardSuggestionFeature,
    &kIPHCookieControlsFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
    // keep-sorted start case=no
    &kIPHGoogleOneOfferNotificationFeature,
    &kIPHGrowthFramework,
    &kIPHLauncherSearchHelpUiFeature,
// keep-sorted end

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    // keep-sorted start case=no
    &kIPHDesktopPWAsLinkCapturingLaunch,
    &kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
    &kIPHSignInBenefitsFeature,
    &kIPHSignInBenefitsNewSigninFeature,
    &kIPHSupervisedUserProfileSigninFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) ||  BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
    // keep-sorted start case=no
    &kIPHSearchPromotionFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN)

};
}  // namespace

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(std::begin(kAllFeatures),
                                           std::end(kAllFeatures));
}

}  // namespace feature_engagement
