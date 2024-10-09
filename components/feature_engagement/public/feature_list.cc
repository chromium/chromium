// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/feature_engagement/public/feature_list.h"

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
    &kIPHAndroidTabDeclutter,
    &kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationAddToBookmarksFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationTranslateFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationReadAloudFeature,
    &kIPHAutoDarkOptOutFeature,
    &kIPHAutoDarkUserEducationMessageFeature,
    &kIPHAutoDarkUserEducationMessageOptInFeature,
    &kIPHAppSpecificHistory,
    &kIPHCCTHistory,
    &kIPHCCTMinimized,
    &kIPHDataSaverDetailFeature,
    &kIPHDataSaverMilestonePromoFeature,
    &kIPHDataSaverPreviewFeature,
    &kIPHDefaultBrowserPromoMagicStackFeature,
    &kIPHDefaultBrowserPromoMessagesFeature,
    &kIPHDefaultBrowserPromoSettingCardFeature,
    &kIPHDownloadHomeFeature,
    &kIPHDownloadIndicatorFeature,
    &kIPHDownloadPageFeature,
    &kIPHDownloadPageScreenshotFeature,
    &kIPHChromeHomeExpandFeature,
    &kIPHChromeHomePullToRefreshFeature,
    &kIPHChromeReengagementNotification1Feature,
    &kIPHChromeReengagementNotification2Feature,
    &kIPHChromeReengagementNotification3Feature,
    &kIPHContextualPageActionsQuietVariantFeature,
    &kIPHContextualPageActionsActionChipFeature,
    &kIPHDownloadSettingsFeature,
    &kIPHDownloadInfoBarDownloadContinuingFeature,
    &kIPHDownloadInfoBarDownloadsAreFasterFeature,
    &kIPHEphemeralTabFeature,
    &kIPHFeedCardMenuFeature,
    &kIPHGenericAlwaysTriggerHelpUiFeature,
    &kIPHIdentityDiscFeature,
    &kIPHInstanceSwitcherFeature,
    &kIPHKeyboardAccessoryAddressFillingFeature,
    &kIPHKeyboardAccessoryBarSwipingFeature,
    &kIPHKeyboardAccessoryPasswordFillingFeature,
    &kIPHKeyboardAccessoryPaymentFillingFeature,
    &kIPHKeyboardAccessoryPaymentOfferFeature,
    &kIPHLowUserEngagementDetectorFeature,
    &kIPHMicToolbarFeature,
    &kIPHPageInfoFeature,
    &kIPHPageInfoStoreInfoFeature,
    &kIPHPageZoomFeature,
    &kIPHPreviewsOmniboxUIFeature,
    &kIPHPriceDropNTPFeature,
    &kIPHQuietNotificationPromptsFeature,
    &kIPHReadAloudAppMenuFeature,
    &kIPHReadAloudExpandedPlayerFeature,
    &kIPHReadLaterContextMenuFeature,
    &kIPHReadLaterAppMenuBookmarkThisPageFeature,
    &kIPHReadLaterAppMenuBookmarksFeature,
    &kIPHReadLaterBottomSheetFeature,
    &kIPHRequestDesktopSiteDefaultOnFeature,
    &kIPHRequestDesktopSiteExceptionsGenericFeature,
    &kIPHRequestDesktopSiteWindowSettingFeature,
    &kIPHShoppingListMenuItemFeature,
    &kIPHShoppingListSaveFlowFeature,
    &kIPHTabGroupsQuicklyComparePagesFeature,
    &kIPHTabGroupsTapToSeeAnotherTabFeature,
    &kIPHTabGroupCreationDialogSyncTextFeature,
    &kIPHTabGroupSyncOnStripFeature,
    &kIPHTabGroupsDragAndDropFeature,
    &kIPHTabGroupsRemoteGroupFeature,
    &kIPHTabGroupsSurfaceFeature,
    &kIPHTabGroupsSurfaceOnHideFeature,
    &kIPHTabSwitcherButtonFeature,
    &kIPHTabSwitcherButtonSwitchIncognitoFeature,
    &kIPHTabSwitcherFloatingActionButtonFeature,
    &kIPHTranslateMenuButtonFeature,
    &kIPHVideoTutorialNTPChromeIntroFeature,
    &kIPHVideoTutorialNTPDownloadFeature,
    &kIPHVideoTutorialNTPSearchFeature,
    &kIPHVideoTutorialNTPVoiceSearchFeature,
    &kIPHVideoTutorialNTPSummaryFeature,
    &kIPHVideoTutorialTryNowFeature,
    &kIPHExploreSitesTileFeature,
    &kIPHFeedHeaderMenuFeature,
    &kIPHWebFeedAwarenessFeature,
    &kIPHFeedSwipeRefresh,
    &kIPHShareScreenshotFeature,
    &kIPHSharingHubLinkToggleFeature,
    &kIPHWebFeedFollowFeature,
    &kIPHWebFeedPostFollowDialogFeature,
    &kIPHWebFeedPostFollowDialogFeatureWithUIUpdate,
    &kIPHSharedHighlightingBuilder,
    &kIPHSharedHighlightingReceiverFeature,
    &kIPHSharingHubWebnotesStylizeFeature,
    &kIPHRestoreTabsOnFREFeature,
    &kIPHRtlGestureNavigationFeature,
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_IOS)
    &kIPHBottomToolbarTipFeature,
    &kIPHLongPressToolbarTipFeature,
    &kIPHBadgedReadingListFeature,
    &kIPHWhatsNewFeature,
    &kIPHWhatsNewUpdatedFeature,
    &kIPHReadingListMessagesFeature,
    &kIPHBadgedTranslateManualTriggerFeature,
    &kIPHDiscoverFeedHeaderFeature,
    &kIPHDefaultSiteViewFeature,
    &kIPHFollowWhileBrowsingFeature,
    &kIPHPriceNotificationsWhileBrowsingFeature,
    &kIPHiOSDefaultBrowserBadgeEligibilityFeature,
    &kIPHiOSDefaultBrowserOverflowMenuBadgeFeature,
    &kIPHiOSLensKeyboardFeature,
    &kIPHiOSPromoAppStoreFeature,
    &kIPHiOSPromoWhatsNewFeature,
    &kIPHiOSPromoPostRestoreFeature,
    &kIPHiOSPromoCredentialProviderExtensionFeature,
    &kIPHiOSPromoDefaultBrowserReminderFeature,
    &kIPHiOSHistoryOnOverflowMenuFeature,
    &kIPHiOSPromoPostRestoreDefaultBrowserFeature,
    &kIPHiOSPromoPasswordManagerWidgetFeature,
    &kIPHiOSParcelTrackingFeature,
    &kIPHiOSPullToRefreshFeature,
    &kIPHiOSReplaceSyncPromosWithSignInPromos,
    &kIPHiOSTabGridSwipeRightForIncognito,
    &kIPHiOSDockingPromoFeature,
    &kIPHiOSDockingPromoRemindMeLaterFeature,
    &kIPHiOSPromoAllTabsFeature,
    &kIPHiOSPromoMadeForIOSFeature,
    &kIPHiOSPromoStaySafeFeature,
    &kIPHiOSSwipeBackForwardFeature,
    &kIPHiOSSwipeToolbarToChangeTabFeature,
    &kIPHiOSPostDefaultAbandonmentPromoFeature,
    &kIPHiOSPromoGenericDefaultBrowserFeature,
    &kIPHiOSOverflowMenuCustomizationFeature,
    &kIPHiOSPageInfoRevampFeature,
    &kIPHiOSInlineEnhancedSafeBrowsingPromoFeature,
    &kIPHiOSSavedTabGroupClosed,
    &kIPHiOSContextualPanelSampleModelFeature,
    &kIPHiOSContextualPanelPriceInsightsFeature,
    &kIPHHomeCustomizationMenuFeature,
    &kIPHiOSLensOverlayEntrypointTipFeature,
#endif  // BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    &kEsbDownloadRowPromoFeature,
#endif
    &kIPHBatterySaverModeFeature,
    &kIPHCompanionSidePanelFeature,
    &kIPHCompanionSidePanelRegionSearchFeature,
    &kIPHComposeMSBBSettingsFeature,
    &kIPHComposeNewBadgeFeature,
    &kIPHDesktopCustomizeChromeFeature,
    &kIPHDesktopCustomizeChromeRefreshFeature,
    &kIPHDesktopNewTabPageModulesCustomizeFeature,
    &kIPHDesktopReEngagementFeature,
    &kIPHDiscardRingFeature,
    &kIPHDownloadEsbPromoFeature,
    &kIPHExperimentalAIPromoFeature,
    &kIPHExplicitBrowserSigninPreferenceRememberedFeature,
    &kIPHHistorySearchFeature,
#if BUILDFLAG(ENABLE_EXTENSIONS)
    &kIPHExtensionsMenuFeature,
    &kIPHExtensionsRequestAccessButtonFeature,
#endif
    &kIPHFocusHelpBubbleScreenReaderPromoFeature,
    &kIPHGMCCastStartStopFeature,
    &kIPHGMCLocalMediaCastingFeature,
    &kIPHMemorySaverModeFeature,
    &kIPHLensOverlayTranslateButtonFeature,
    &kIPHLiveCaptionFeature,
    &kIPHTabAudioMutingFeature,
    &kIPHPasswordsManagementBubbleAfterSaveFeature,
    &kIPHPasswordsManagementBubbleDuringSigninFeature,
    &kIPHPasswordsWebAppProfileSwitchFeature,
    &kIPHPasswordManagerShortcutFeature,
    &kIPHPasswordSharingFeature,
    &kIPHPerformanceInterventionDialogFeature,
    &kIPHPowerBookmarksSidePanelFeature,
    &kIPHPriceInsightsPageActionIconLabelFeature,
    &kIPHPriceTrackingEmailConsentFeature,
    &kIPHPriceTrackingPageActionIconLabelFeature,
    &kIPHReadingListDiscoveryFeature,
    &kIPHReadingListEntryPointFeature,
    &kIPHReadingListInSidePanelFeature,
    &kIPHReadingModeSidePanelFeature,
    &kIPHShoppingCollectionFeature,
    &kIPHSidePanelGenericPinnableFeature,
    &kIPHSidePanelLensOverlayPinnableFeature,
    &kIPHSidePanelLensOverlayPinnableFollowupFeature,
    &kIPHSideSearchAutoTriggeringFeature,
    &kIPHSideSearchPageActionLabelFeature,
    &kIPHSignoutWebInterceptFeature,
    &kIPHSupervisedUserProfileSigninFeature,
    &kIPHTabGroupsSaveV2IntroFeature,
    &kIPHTabGroupsSaveV2CloseGroupFeature,
    &kIPHTabOrganizationSuccessFeature,
    &kIPHTabSearchFeature,
    &kIPHWebUITabStripFeature,
    &kIPHDesktopPwaInstallFeature,
    &kIPHProfileSwitchFeature,
    &kIPHDesktopSharedHighlightingFeature,
    &kIPHWebUiHelpBubbleTestFeature,
    &kIPHPriceTrackingInSidePanelFeature,
    &kIPHBackNavigationMenuFeature,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
    &kIPHAutofillCreditCardBenefitFeature,
    &kIPHAutofillDisabledVirtualCardSuggestionFeature,
    &kIPHAutofillExternalAccountProfileSuggestionFeature,
    &kIPHAutofillManualFallbackFeature,
    &kIPHAutofillPredictionImprovementsFeature,
    &kIPHAutofillVirtualCardCVCSuggestionFeature,
    &kIPHAutofillVirtualCardSuggestionFeature,
    &kIPHCookieControlsFeature,
    &kIPHPlusAddressCreateSuggestionFeature,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    &kIPHGrowthFramework,
    &kIPHGoogleOneOfferNotificationFeature,
    &kIPHLauncherSearchHelpUiFeature,
    &kIPHScalableIphTimerBasedOneFeature,
    &kIPHScalableIphTimerBasedTwoFeature,
    &kIPHScalableIphTimerBasedThreeFeature,
    &kIPHScalableIphTimerBasedFourFeature,
    &kIPHScalableIphTimerBasedFiveFeature,
    &kIPHScalableIphTimerBasedSixFeature,
    &kIPHScalableIphTimerBasedSevenFeature,
    &kIPHScalableIphTimerBasedEightFeature,
    &kIPHScalableIphTimerBasedNineFeature,
    &kIPHScalableIphTimerBasedTenFeature,
    &kIPHScalableIphUnlockedBasedOneFeature,
    &kIPHScalableIphUnlockedBasedTwoFeature,
    &kIPHScalableIphUnlockedBasedThreeFeature,
    &kIPHScalableIphUnlockedBasedFourFeature,
    &kIPHScalableIphUnlockedBasedFiveFeature,
    &kIPHScalableIphUnlockedBasedSixFeature,
    &kIPHScalableIphUnlockedBasedSevenFeature,
    &kIPHScalableIphUnlockedBasedEightFeature,
    &kIPHScalableIphUnlockedBasedNineFeature,
    &kIPHScalableIphUnlockedBasedTenFeature,
    &kIPHScalableIphHelpAppBasedNudgeFeature,
    &kIPHScalableIphHelpAppBasedOneFeature,
    &kIPHScalableIphHelpAppBasedTwoFeature,
    &kIPHScalableIphHelpAppBasedThreeFeature,
    &kIPHScalableIphHelpAppBasedFourFeature,
    &kIPHScalableIphHelpAppBasedFiveFeature,
    &kIPHScalableIphHelpAppBasedSixFeature,
    &kIPHScalableIphHelpAppBasedSevenFeature,
    &kIPHScalableIphHelpAppBasedEightFeature,
    &kIPHScalableIphHelpAppBasedNineFeature,
    &kIPHScalableIphHelpAppBasedTenFeature,
    &kIPHScalableIphGamingFeature,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    &kIPHDesktopPWAsLinkCapturingLaunch,
    &kIPHToolbarManagementButtonFeature,
#endif  // BUILDFLAG(IS_WIN) ||  BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
    &kIPHiOSPasswordPromoDesktopFeature,
    &kIPHiOSAddressPromoDesktopFeature,
    &kIPHiOSPaymentPromoDesktopFeature
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
};
}  // namespace

const char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + std::size(kAllFeatures));
}

}  // namespace feature_engagement
