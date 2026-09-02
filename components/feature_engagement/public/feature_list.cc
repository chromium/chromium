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
    // ALL_FEATURES_ANDROID_START
    // keep-sorted start case=no
    &kIPHAccountSettingsHistorySync,
    &kIPHAdaptiveButtonInTopToolbarCustomizationAddToBookmarksFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationOpenInBrowserFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryPdfFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryWebFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationReadAloudFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationTranslateFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature,
    &kIPHAdaptiveButtonPinGlicToolbarButtonFeature,
    &kIPHAimActivationHint,
    &kIPHAndroidBottomBarAim,
    &kIPHAndroidBottomBarAimPromoDialog,
    &kIPHAndroidBottomBarGlic,
    &kIPHAndroidBottomBarNewTab,
    &kIPHAndroidBottomBarPromoDialog,
    &kIPHAndroidTabDeclutter,
    &kIPHAndroidVerticalTabsPromoFeature,
    &kIPHAppRatingPromptFeature,
    &kIPHAppSpecificHistory,
    &kIPHAutoDarkOptOutFeature,
    &kIPHAutoDarkUserEducationMessageFeature,
    &kIPHAutoDarkUserEducationMessageOptInFeature,
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
    &kIPHIncognitoIndicatorCloseAllWindows,
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
    &kIPHRecentTabsFeature,
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
    // keep-sorted end
// ALL_FEATURES_ANDROID_END
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
    &kIPHSendTabToSelfOmnibox,
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
    &kIPHiOSBackendPromoFeature,
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
    &kIPHiOSGeminiLiveIPHFeature,
    &kIPHiOSGeminiLiveNewBadgeFeature,
    &kIPHiOSGeminiWhatCanGeminiDo,
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
    &kIPHiOSPromoOverflowMenuDestinationDefaultBrowserFeature,
    &kIPHiOSPromoOverflowMenuShortcutsDefaultBrowserFeature,
    &kIPHiOSPromoPasswordManagerWidgetFeature,
    &kIPHiOSPromoPostRestoreDefaultBrowserFeature,
    &kIPHiOSPromoPostRestoreFeature,
    &kIPHiOSPromoSettingsCardDefaultBrowserFeature,
    &kIPHiOSPromoSettingsCellDefaultBrowserFeature,
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
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    &kIPHExtensionsMenuFeature,
    &kIPHExtensionsRequestAccessButtonFeature,
    &kIPHExtensionsZeroStatePromoFeature,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    // keep-sorted start case=no
    &kIPHBackNavigationMenuFeature,
    &kIPHBatterySaverModeFeature,
    &kIPHBookmarkBarSimplifiedFeature,
    &kIPHCompanionSidePanelFeature,
    &kIPHCompanionSidePanelRegionSearchFeature,
    &kIPHComposeMSBBSettingsFeature,
    &kIPHComposeNewBadgeFeature,
    &kIPHContextualTasksEphemeralToolbarButtonFeature,
    &kIPHCriticalActionAppMenuFeature,
    &kIPHCriticalActionFilterChipFeature,
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
    &kIPHGMCSaveVideoFrameFeature,
    &kIPHHistorySearchFeature,
    &kIPHLensOverlayFeature,
    &kIPHLensOverlayTranslateButtonFeature,
    &kIPHMemorySaverModeFeature,
    &kIPHMultistepFilterPromoFeature,
    &kIPHOmniboxEverywhereLensPromoFeature,
    &kIPHPasswordManagerShortcutFeature,
    &kIPHPasswordSharingFeature,
    &kIPHPasswordsManagementBubbleAfterSaveFeature,
    &kIPHPasswordsManagementBubbleDuringSigninFeature,
    &kIPHPasswordsSavePrimingPromoFeature,
    &kIPHPasswordsSaveRecoveryPromoFeature,
    &kIPHPasswordsWebAppProfileSwitchFeature,
    &kIPHPdfGlicSummarizeFeature,
    &kIPHPdfInkSignaturesFeature,
    &kIPHPdfSearchifyFeature,
    &kIPHPdfTextAnnotationsFeature,
    &kIPHPdfTranslateBubbleFeature,
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
    &kIPHReadingModePresentationModeFeature,
    &kIPHReadingModeSidePanelFeature,
    &kIPHSendTabToSelfTutorialFeature,
    &kIPHShoppingCollectionFeature,
    &kIPHSideBySidePinnableFeature,
    &kIPHSideBySideTabSwitchFeature,
    &kIPHSidePanelContextualTasksPinnableFeature,
    &kIPHSidePanelGenericPinnableFeature,
    &kIPHSidePanelLensOverlayPinnableFeature,
    &kIPHSidePanelLensOverlayPinnableFollowupFeature,
    &kIPHSideSearchAutoTriggeringFeature,
    &kIPHSideSearchPageActionLabelFeature,
    &kIPHSmartTabSharingDefaultOnFeature,
    &kIPHSmartTabSharingFeature,
    &kIPHSmartTabSharingTryItFeature,
    &kIPHSplitViewHorizontalIndirectAccessFeature,
    &kIPHTabAudioMutingFeature,
    &kIPHTabGroupsSaveV2CloseGroupFeature,
    &kIPHTabGroupsSaveV2IntroFeature,
    &kIPHTabGroupsSharedTabChangedFeature,
    &kIPHTabGroupsSharedTabFeedbackFeature,
    &kIPHTabScrollButtonFeature,
    &kIPHTabSearchComboButtonFeature,
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
    &kIPHAutofillOmniboxPaymentChipFeature,
    &kIPHAutofillVirtualCardCVCSuggestionFeature,
    &kIPHAutofillVirtualCardSuggestionFeature,
    &kIPHAutofillWalletDirectOffersFeature,
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    // keep-sorted start case=no
    &kIPHDesktopPWAsLinkCapturingLaunch,
    &kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    &kIPHExtensionsPinnedByDefaultFeature,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    // keep-sorted start case=no
    &kIPHSignInBenefitsFeature,
    &kIPHSignInBenefitsNewSigninFeature,
    &kIPHSupervisedUserProfileSigninFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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
