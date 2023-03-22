// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    &kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationAddToBookmarksFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationTranslateFeature,
    &kIPHAddToHomescreenMessageFeature,
    &kIPHAutoDarkOptOutFeature,
    &kIPHAutoDarkUserEducationMessageFeature,
    &kIPHAutoDarkUserEducationMessageOptInFeature,
    &kIPHDataSaverDetailFeature,
    &kIPHDataSaverMilestonePromoFeature,
    &kIPHDataSaverPreviewFeature,
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
    &kIPHFeatureNotificationGuideDefaultBrowserNotificationShownFeature,
    &kIPHFeatureNotificationGuideSignInNotificationShownFeature,
    &kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature,
    &kIPHFeatureNotificationGuideNTPSuggestionCardNotificationShownFeature,
    &kIPHFeatureNotificationGuideVoiceSearchNotificationShownFeature,
    &kIPHFeatureNotificationGuideDefaultBrowserPromoFeature,
    &kIPHFeatureNotificationGuideSignInHelpBubbleFeature,
    &kIPHFeatureNotificationGuideIncognitoTabHelpBubbleFeature,
    &kIPHFeatureNotificationGuideVoiceSearchHelpBubbleFeature,
    &kIPHFeatureNotificationGuideNTPSuggestionCardHelpBubbleFeature,
    &kIPHFeatureNotificationGuideIncognitoTabUsedFeature,
    &kIPHFeatureNotificationGuideVoiceSearchUsedFeature,
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
    &kIPHNewTabPageHomeButtonFeature,
    &kIPHPageInfoFeature,
    &kIPHPageInfoStoreInfoFeature,
    &kIPHPageZoomFeature,
    &kIPHPreviewsOmniboxUIFeature,
    &kIPHPriceDropNTPFeature,
    &kIPHPwaInstallAvailableFeature,
    &kIPHQuietNotificationPromptsFeature,
    &kIPHReadLaterContextMenuFeature,
    &kIPHReadLaterAppMenuBookmarkThisPageFeature,
    &kIPHReadLaterAppMenuBookmarksFeature,
    &kIPHReadLaterBottomSheetFeature,
    &kIPHRequestDesktopSiteAppMenuFeature,
    &kIPHRequestDesktopSiteDefaultOnFeature,
    &kIPHRequestDesktopSiteOptInFeature,
    &kIPHRequestDesktopSiteExceptionsGenericFeature,
    &kIPHRequestDesktopSiteExceptionsSpecificFeature,
    &kIPHShoppingListMenuItemFeature,
    &kIPHShoppingListSaveFlowFeature,
    &kIPHTabGroupsQuicklyComparePagesFeature,
    &kIPHTabGroupsTapToSeeAnotherTabFeature,
    &kIPHTabGroupsYourTabsAreTogetherFeature,
    &kIPHTabGroupsDragAndDropFeature,
    &kIPHTabSwitcherButtonFeature,
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
    &kIPHSharedHighlightingBuilder,
    &kIPHSharedHighlightingReceiverFeature,
    &kIPHSharingHubWebnotesStylizeFeature,
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_IOS)
    &kIPHBottomToolbarTipFeature,
    &kIPHLongPressToolbarTipFeature,
    &kIPHNewTabTipFeature,
    &kIPHNewIncognitoTabTipFeature,
    &kIPHBadgedReadingListFeature,
    &kIPHWhatsNewFeature,
    &kIPHReadingListMessagesFeature,
    &kIPHBadgedTranslateManualTriggerFeature,
    &kIPHDiscoverFeedHeaderFeature,
    &kIPHDefaultSiteViewFeature,
    &kIPHFollowWhileBrowsingFeature,
    &kIPHOverflowMenuTipFeature,
    &kIPHPriceNotificationsWhileBrowsingFeature,
    &kIPHiOSDefaultBrowserBadgeEligibilityFeature,
    &kIPHiOSDefaultBrowserOverflowMenuBadgeFeature,
    &kIPHiOSDefaultBrowserSettingsBadgeFeature,
    &kIPHiOSPromoAppStoreFeature,
    &kIPHTabPinnedFeature,
    &kIPHiOSPromoWhatsNewFeature,
    &kIPHiOSPromoPostRestoreFeature,
    &kIPHiOSPromoCredentialProviderExtensionFeature,
#endif  // BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
    &kIPHBatterySaverModeFeature,
    &kIPHDesktopTabGroupsNewGroupFeature,
    &kIPHDesktopCustomizeChromeFeature,
    &kIPHDownloadToolbarButtonFeature,
    &kIPHExtensionsMenuFeature,
    &kIPHFocusHelpBubbleScreenReaderPromoFeature,
    &kIPHGMCCastStartStopFeature,
    &kIPHHighEfficiencyInfoModeFeature,
    &kIPHHighEfficiencyModeFeature,
    &kIPHLiveCaptionFeature,
    &kIPHTabAudioMutingFeature,
    &kIPHPasswordsAccountStorageFeature,
    &kIPHPasswordsManagementBubbleAfterSaveFeature,
    &kIPHPasswordsManagementBubbleDuringSigninFeature,
    &kIPHPasswordsWebAppProfileSwitchFeature,
    &kIPHPerformanceNewBadgeFeature,
    &kIPHPowerBookmarksSidePanelFeature,
    &kIPHPriceTrackingPageActionIconLabelFeature,
    &kIPHReadingListDiscoveryFeature,
    &kIPHReadingListEntryPointFeature,
    &kIPHReadingListInSidePanelFeature,
    &kIPHReopenTabFeature,
    &kIPHSideSearchAutoTriggeringFeature,
    &kIPHSideSearchFeature,
    &kIPHSideSearchPageActionLabelFeature,
    &kIPHTabSearchFeature,
    &kIPHWebUITabStripFeature,
    &kIPHDesktopPwaInstallFeature,
    &kIPHProfileSwitchFeature,
    &kIPHDesktopSharedHighlightingFeature,
    &kIPHIntentChipFeature,
    &kIPHWebUiHelpBubbleTestFeature,
    &kIPHPriceTrackingInSidePanelFeature,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
    &kIPHAutofillVirtualCardSuggestionFeature,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    &kIPHGoogleOneOfferNotificationFeature,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};
}  // namespace

const char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + std::size(kAllFeatures));
}

}  // namespace feature_engagement
