// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace feature_engagement {

// A feature for enabling automatic snooze mode for In-Product Help (IPH). When
// this flag is enabled, we don't show snooze button/UI on the IPH, but on
// dismiss we will implicitly snooze it until the snooze limit count is reached.
BASE_DECLARE_FEATURE(kEnableAutomaticSnooze);

// A feature for enabling a demonstration mode for In-Product Help (IPH).
BASE_DECLARE_FEATURE(kIPHDemoMode);

// A feature for enabling a snooze mode for In-Product Help (IPH).
BASE_DECLARE_FEATURE(kIPHSnooze);

// A feature for enabling In-Product Help (IPH) to use client side
// configuration. When this flag is enabled, finch config will be ignored for
// all IPHs.
BASE_DECLARE_FEATURE(kUseClientConfigIPH);

// A feature to ensure all arrays can contain at least one feature.
BASE_DECLARE_FEATURE(kIPHDummyFeature);

BASE_DECLARE_FEATURE(kEnableIPH);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
BASE_DECLARE_FEATURE(kIPHBatterySaverModeFeature);
BASE_DECLARE_FEATURE(kIPHDesktopSharedHighlightingFeature);
BASE_DECLARE_FEATURE(kIPHDesktopTabGroupsNewGroupFeature);
BASE_DECLARE_FEATURE(kIPHExtensionsMenuFeature);
BASE_DECLARE_FEATURE(kIPHFocusHelpBubbleScreenReaderPromoFeature);
BASE_DECLARE_FEATURE(kIPHGMCCastStartStopFeature);
BASE_DECLARE_FEATURE(kIPHHighEfficiencyInfoModeFeature);
BASE_DECLARE_FEATURE(kIPHHighEfficiencyModeFeature);
BASE_DECLARE_FEATURE(kIPHLiveCaptionFeature);
BASE_DECLARE_FEATURE(kIPHTabAudioMutingFeature);
BASE_DECLARE_FEATURE(kIPHPasswordsAccountStorageFeature);
BASE_DECLARE_FEATURE(kIPHPerformanceNewBadgeFeature);
BASE_DECLARE_FEATURE(kIPHPriceTrackingPageActionIconLabelFeature);
BASE_DECLARE_FEATURE(kIPHReadingListDiscoveryFeature);
BASE_DECLARE_FEATURE(kIPHReadingListEntryPointFeature);
BASE_DECLARE_FEATURE(kIPHIntentChipFeature);
BASE_DECLARE_FEATURE(kIPHReadingListInSidePanelFeature);
BASE_DECLARE_FEATURE(kIPHReopenTabFeature);
BASE_DECLARE_FEATURE(kIPHSideSearchAutoTriggeringFeature);
BASE_DECLARE_FEATURE(kIPHSideSearchFeature);
BASE_DECLARE_FEATURE(kIPHSideSearchPageActionLabelFeature);
BASE_DECLARE_FEATURE(kIPHTabSearchFeature);
BASE_DECLARE_FEATURE(kIPHWebUITabStripFeature);
BASE_DECLARE_FEATURE(kIPHDesktopSnoozeFeature);
BASE_DECLARE_FEATURE(kIPHDesktopPwaInstallFeature);
BASE_DECLARE_FEATURE(kIPHProfileSwitchFeature);
BASE_DECLARE_FEATURE(kIPHWebUiHelpBubbleTestFeature);
BASE_DECLARE_FEATURE(kIPHPriceTrackingInSidePanelFeature);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

// All the features declared for Android below that are also used in Java,
// should also be declared in:
// org.chromium.components.feature_engagement.FeatureConstants.
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature);
BASE_DECLARE_FEATURE(kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature);
BASE_DECLARE_FEATURE(
    kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature);
BASE_DECLARE_FEATURE(kIPHAddToHomescreenMessageFeature);
BASE_DECLARE_FEATURE(kIPHAutoDarkOptOutFeature);
BASE_DECLARE_FEATURE(kIPHAutoDarkUserEducationMessageFeature);
BASE_DECLARE_FEATURE(kIPHAutoDarkUserEducationMessageOptInFeature);
BASE_DECLARE_FEATURE(kIPHContextualPageActionsQuietVariantFeature);
BASE_DECLARE_FEATURE(kIPHContextualPageActionsActionChipFeature);
BASE_DECLARE_FEATURE(kIPHCrowFeature);
BASE_DECLARE_FEATURE(kIPHDataSaverDetailFeature);
BASE_DECLARE_FEATURE(kIPHDataSaverMilestonePromoFeature);
BASE_DECLARE_FEATURE(kIPHDataSaverPreviewFeature);
BASE_DECLARE_FEATURE(kIPHDownloadHomeFeature);
BASE_DECLARE_FEATURE(kIPHDownloadIndicatorFeature);
BASE_DECLARE_FEATURE(kIPHDownloadPageFeature);
BASE_DECLARE_FEATURE(kIPHDownloadPageScreenshotFeature);
BASE_DECLARE_FEATURE(kIPHChromeHomeExpandFeature);
BASE_DECLARE_FEATURE(kIPHChromeHomePullToRefreshFeature);
BASE_DECLARE_FEATURE(kIPHDownloadSettingsFeature);
BASE_DECLARE_FEATURE(kIPHDownloadInfoBarDownloadContinuingFeature);
BASE_DECLARE_FEATURE(kIPHDownloadInfoBarDownloadsAreFasterFeature);
BASE_DECLARE_FEATURE(kIPHEphemeralTabFeature);
BASE_DECLARE_FEATURE(
    kIPHFeatureNotificationGuideDefaultBrowserNotificationShownFeature);
BASE_DECLARE_FEATURE(
    kIPHFeatureNotificationGuideSignInNotificationShownFeature);
BASE_DECLARE_FEATURE(
    kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature);
BASE_DECLARE_FEATURE(
    kIPHFeatureNotificationGuideNTPSuggestionCardNotificationShownFeature);
BASE_DECLARE_FEATURE(
    kIPHFeatureNotificationGuideVoiceSearchNotificationShownFeature);
BASE_DECLARE_FEATURE(kIPHFeatureNotificationGuideDefaultBrowserPromoFeature);
BASE_DECLARE_FEATURE(kIPHFeatureNotificationGuideSignInHelpBubbleFeature);
BASE_DECLARE_FEATURE(kIPHFeatureNotificationGuideIncognitoTabHelpBubbleFeature);
BASE_DECLARE_FEATURE(
    kIPHFeatureNotificationGuideNTPSuggestionCardHelpBubbleFeature);
BASE_DECLARE_FEATURE(kIPHFeatureNotificationGuideVoiceSearchHelpBubbleFeature);
BASE_DECLARE_FEATURE(kIPHFeatureNotificationGuideIncognitoTabUsedFeature);
BASE_DECLARE_FEATURE(kIPHFeatureNotificationGuideVoiceSearchUsedFeature);
BASE_DECLARE_FEATURE(kIPHFeedCardMenuFeature);
BASE_DECLARE_FEATURE(kIPHGenericAlwaysTriggerHelpUiFeature);
BASE_DECLARE_FEATURE(kIPHHomePageButtonFeature);
BASE_DECLARE_FEATURE(kIPHHomepageTileFeature);
BASE_DECLARE_FEATURE(kIPHIdentityDiscFeature);
BASE_DECLARE_FEATURE(kIPHInstanceSwitcherFeature);
BASE_DECLARE_FEATURE(kIPHKeyboardAccessoryAddressFillingFeature);
BASE_DECLARE_FEATURE(kIPHKeyboardAccessoryBarSwipingFeature);
BASE_DECLARE_FEATURE(kIPHKeyboardAccessoryPasswordFillingFeature);
BASE_DECLARE_FEATURE(kIPHKeyboardAccessoryPaymentFillingFeature);
BASE_DECLARE_FEATURE(kIPHKeyboardAccessoryPaymentOfferFeature);
BASE_DECLARE_FEATURE(kIPHLowUserEngagementDetectorFeature);
BASE_DECLARE_FEATURE(kIPHMicToolbarFeature);
BASE_DECLARE_FEATURE(kIPHNewTabPageHomeButtonFeature);
BASE_DECLARE_FEATURE(kIPHPageInfoFeature);
BASE_DECLARE_FEATURE(kIPHPageInfoStoreInfoFeature);
BASE_DECLARE_FEATURE(kIPHPreviewsOmniboxUIFeature);
BASE_DECLARE_FEATURE(kIPHPriceDropNTPFeature);
BASE_DECLARE_FEATURE(kIPHQuietNotificationPromptsFeature);
BASE_DECLARE_FEATURE(kIPHReadLaterContextMenuFeature);
BASE_DECLARE_FEATURE(kIPHReadLaterAppMenuBookmarkThisPageFeature);
BASE_DECLARE_FEATURE(kIPHReadLaterAppMenuBookmarksFeature);
BASE_DECLARE_FEATURE(kIPHReadLaterBottomSheetFeature);
BASE_DECLARE_FEATURE(kIPHRequestDesktopSiteAppMenuFeature);
BASE_DECLARE_FEATURE(kIPHRequestDesktopSiteDefaultOnFeature);
BASE_DECLARE_FEATURE(kIPHRequestDesktopSiteOptInFeature);
BASE_DECLARE_FEATURE(kIPHShoppingListMenuItemFeature);
BASE_DECLARE_FEATURE(kIPHShoppingListSaveFlowFeature);
BASE_DECLARE_FEATURE(kIPHTabGroupsQuicklyComparePagesFeature);
BASE_DECLARE_FEATURE(kIPHTabGroupsTapToSeeAnotherTabFeature);
BASE_DECLARE_FEATURE(kIPHTabGroupsYourTabsAreTogetherFeature);
BASE_DECLARE_FEATURE(kIPHTabGroupsDragAndDropFeature);
BASE_DECLARE_FEATURE(kIPHTabSwitcherButtonFeature);
BASE_DECLARE_FEATURE(kIPHTranslateMenuButtonFeature);
BASE_DECLARE_FEATURE(kIPHVideoTutorialNTPChromeIntroFeature);
BASE_DECLARE_FEATURE(kIPHVideoTutorialNTPDownloadFeature);
BASE_DECLARE_FEATURE(kIPHVideoTutorialNTPSearchFeature);
BASE_DECLARE_FEATURE(kIPHVideoTutorialNTPVoiceSearchFeature);
BASE_DECLARE_FEATURE(kIPHVideoTutorialNTPSummaryFeature);
BASE_DECLARE_FEATURE(kIPHVideoTutorialTryNowFeature);
BASE_DECLARE_FEATURE(kIPHExploreSitesTileFeature);
BASE_DECLARE_FEATURE(kIPHFeedHeaderMenuFeature);
BASE_DECLARE_FEATURE(kIPHWebFeedAwarenessFeature);
BASE_DECLARE_FEATURE(kIPHFeedSwipeRefresh);
BASE_DECLARE_FEATURE(kIPHChromeReengagementNotification1Feature);
BASE_DECLARE_FEATURE(kIPHChromeReengagementNotification2Feature);
BASE_DECLARE_FEATURE(kIPHChromeReengagementNotification3Feature);
BASE_DECLARE_FEATURE(kIPHPwaInstallAvailableFeature);
BASE_DECLARE_FEATURE(kIPHShareScreenshotFeature);
BASE_DECLARE_FEATURE(kIPHSharingHubLinkToggleFeature);
BASE_DECLARE_FEATURE(kIPHWebFeedFollowFeature);
BASE_DECLARE_FEATURE(kIPHWebFeedPostFollowDialogFeature);
BASE_DECLARE_FEATURE(kIPHSharedHighlightingBuilder);
BASE_DECLARE_FEATURE(kIPHSharedHighlightingReceiverFeature);
BASE_DECLARE_FEATURE(kIPHSharingHubWebnotesStylizeFeature);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kIPHBottomToolbarTipFeature);
BASE_DECLARE_FEATURE(kIPHLongPressToolbarTipFeature);
BASE_DECLARE_FEATURE(kIPHNewTabTipFeature);
BASE_DECLARE_FEATURE(kIPHNewIncognitoTabTipFeature);
BASE_DECLARE_FEATURE(kIPHBadgedReadingListFeature);
BASE_DECLARE_FEATURE(kIPHWhatsNewFeature);
BASE_DECLARE_FEATURE(kIPHReadingListMessagesFeature);
BASE_DECLARE_FEATURE(kIPHBadgedTranslateManualTriggerFeature);
BASE_DECLARE_FEATURE(kIPHDiscoverFeedHeaderFeature);
BASE_DECLARE_FEATURE(kIPHDefaultSiteViewFeature);
BASE_DECLARE_FEATURE(kIPHPasswordSuggestionsFeature);
BASE_DECLARE_FEATURE(kIPHFollowWhileBrowsingFeature);
BASE_DECLARE_FEATURE(kIPHOverflowMenuTipFeature);
BASE_DECLARE_FEATURE(kIPHPriceNotificationsWhileBrowsingFeature);
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
BASE_DECLARE_FEATURE(kIPHAutofillVirtualCardSuggestionFeature);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
