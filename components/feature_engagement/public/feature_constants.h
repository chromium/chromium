// Copyright 2017 The Chromium Authors. All rights reserved.
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
extern const base::Feature kEnableAutomaticSnooze;

// A feature for enabling a demonstration mode for In-Product Help (IPH).
extern const base::Feature kIPHDemoMode;

// A feature for enabling a snooze mode for In-Product Help (IPH).
extern const base::Feature kIPHSnooze;

// A feature for enabling In-Product Help (IPH) to use client side
// configuration. When this flag is enabled, finch config will be ignored for
// all IPHs.
extern const base::Feature kUseClientConfigIPH;

// A feature to ensure all arrays can contain at least one feature.
extern const base::Feature kIPHDummyFeature;

extern const base::Feature kEnableIPH;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
extern const base::Feature kIPHDesktopSharedHighlightingFeature;
extern const base::Feature kIPHDesktopTabGroupsNewGroupFeature;
extern const base::Feature kIPHFocusHelpBubbleScreenReaderPromoFeature;
extern const base::Feature kIPHGMCCastStartStopFeature;
extern const base::Feature kIPHLiveCaptionFeature;
extern const base::Feature kIPHTabAudioMutingFeature;
extern const base::Feature kIPHPasswordsAccountStorageFeature;
extern const base::Feature kIPHReadingListDiscoveryFeature;
extern const base::Feature kIPHReadingListEntryPointFeature;
extern const base::Feature kIPHIntentChipFeature;
extern const base::Feature kIPHReadingListInSidePanelFeature;
extern const base::Feature kIPHReopenTabFeature;
extern const base::Feature kIPHSideSearchFeature;
extern const base::Feature kIPHTabSearchFeature;
extern const base::Feature kIPHWebUITabStripFeature;
extern const base::Feature kIPHDesktopSnoozeFeature;
extern const base::Feature kIPHDesktopPwaInstallFeature;
extern const base::Feature kIPHProfileSwitchFeature;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

// All the features declared for Android below that are also used in Java,
// should also be declared in:
// org.chromium.components.feature_engagement.FeatureConstants.
#if BUILDFLAG(IS_ANDROID)
extern const base::Feature
    kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature;
extern const base::Feature
    kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature;
extern const base::Feature
    kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature;
extern const base::Feature kIPHAddToHomescreenMessageFeature;
extern const base::Feature kIPHAutoDarkOptOutFeature;
extern const base::Feature kIPHAutoDarkUserEducationMessageFeature;
extern const base::Feature kIPHAutoDarkUserEducationMessageOptInFeature;
extern const base::Feature kIPHContextualPageActionsPriceTrackingFeature;
extern const base::Feature kIPHCrowFeature;
extern const base::Feature kIPHDataSaverDetailFeature;
extern const base::Feature kIPHDataSaverMilestonePromoFeature;
extern const base::Feature kIPHDataSaverPreviewFeature;
extern const base::Feature kIPHDownloadHomeFeature;
extern const base::Feature kIPHDownloadIndicatorFeature;
extern const base::Feature kIPHDownloadPageFeature;
extern const base::Feature kIPHDownloadPageScreenshotFeature;
extern const base::Feature kIPHChromeHomeExpandFeature;
extern const base::Feature kIPHChromeHomePullToRefreshFeature;
extern const base::Feature kIPHContextualSearchTranslationEnableFeature;
extern const base::Feature kIPHContextualSearchWebSearchFeature;
extern const base::Feature kIPHContextualSearchPromoteTapFeature;
extern const base::Feature kIPHContextualSearchPromotePanelOpenFeature;
extern const base::Feature kIPHContextualSearchOptInFeature;
extern const base::Feature kIPHContextualSearchTappedButShouldLongpressFeature;
extern const base::Feature kIPHContextualSearchInPanelHelpFeature;
extern const base::Feature kIPHDownloadSettingsFeature;
extern const base::Feature kIPHDownloadInfoBarDownloadContinuingFeature;
extern const base::Feature kIPHDownloadInfoBarDownloadsAreFasterFeature;
extern const base::Feature kIPHEphemeralTabFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideDefaultBrowserNotificationShownFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideSignInNotificationShownFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideNTPSuggestionCardNotificationShownFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideVoiceSearchNotificationShownFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideDefaultBrowserPromoFeature;
extern const base::Feature kIPHFeatureNotificationGuideSignInHelpBubbleFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideIncognitoTabHelpBubbleFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideNTPSuggestionCardHelpBubbleFeature;
extern const base::Feature
    kIPHFeatureNotificationGuideVoiceSearchHelpBubbleFeature;
extern const base::Feature kIPHFeatureNotificationGuideIncognitoTabUsedFeature;
extern const base::Feature kIPHFeatureNotificationGuideVoiceSearchUsedFeature;
extern const base::Feature kIPHFeedCardMenuFeature;
extern const base::Feature kIPHGenericAlwaysTriggerHelpUiFeature;
extern const base::Feature kIPHHomePageButtonFeature;
extern const base::Feature kIPHHomepageTileFeature;
extern const base::Feature kIPHIdentityDiscFeature;
extern const base::Feature kIPHInstanceSwitcherFeature;
extern const base::Feature kIPHKeyboardAccessoryAddressFillingFeature;
extern const base::Feature kIPHKeyboardAccessoryBarSwipingFeature;
extern const base::Feature kIPHKeyboardAccessoryPasswordFillingFeature;
extern const base::Feature kIPHKeyboardAccessoryPaymentFillingFeature;
extern const base::Feature kIPHKeyboardAccessoryPaymentOfferFeature;
extern const base::Feature kIPHLowUserEngagementDetectorFeature;
extern const base::Feature kIPHMicToolbarFeature;
extern const base::Feature kIPHNewTabPageHomeButtonFeature;
extern const base::Feature kIPHPageInfoFeature;
extern const base::Feature kIPHPageInfoStoreInfoFeature;
extern const base::Feature kIPHPreviewsOmniboxUIFeature;
extern const base::Feature kIPHPriceDropNTPFeature;
extern const base::Feature kIPHQuietNotificationPromptsFeature;
extern const base::Feature kIPHReadLaterContextMenuFeature;
extern const base::Feature kIPHReadLaterAppMenuBookmarkThisPageFeature;
extern const base::Feature kIPHReadLaterAppMenuBookmarksFeature;
extern const base::Feature kIPHReadLaterBottomSheetFeature;
extern const base::Feature kIPHShoppingListMenuItemFeature;
extern const base::Feature kIPHShoppingListSaveFlowFeature;
extern const base::Feature kIPHTabGroupsQuicklyComparePagesFeature;
extern const base::Feature kIPHTabGroupsTapToSeeAnotherTabFeature;
extern const base::Feature kIPHTabGroupsYourTabsAreTogetherFeature;
extern const base::Feature kIPHTabGroupsDragAndDropFeature;
extern const base::Feature kIPHTabSwitcherButtonFeature;
extern const base::Feature kIPHTranslateMenuButtonFeature;
extern const base::Feature kIPHVideoTutorialNTPChromeIntroFeature;
extern const base::Feature kIPHVideoTutorialNTPDownloadFeature;
extern const base::Feature kIPHVideoTutorialNTPSearchFeature;
extern const base::Feature kIPHVideoTutorialNTPVoiceSearchFeature;
extern const base::Feature kIPHVideoTutorialNTPSummaryFeature;
extern const base::Feature kIPHVideoTutorialTryNowFeature;
extern const base::Feature kIPHExploreSitesTileFeature;
extern const base::Feature kIPHFeedHeaderMenuFeature;
extern const base::Feature kIPHWebFeedAwarenessFeature;
extern const base::Feature kIPHFeedSwipeRefresh;
extern const base::Feature kIPHChromeReengagementNotification1Feature;
extern const base::Feature kIPHChromeReengagementNotification2Feature;
extern const base::Feature kIPHChromeReengagementNotification3Feature;
extern const base::Feature kIPHPwaInstallAvailableFeature;
extern const base::Feature kIPHShareScreenshotFeature;
extern const base::Feature kIPHSharingHubLinkToggleFeature;
extern const base::Feature kIPHWebFeedFollowFeature;
extern const base::Feature kIPHWebFeedPostFollowDialogFeature;
extern const base::Feature kIPHSharedHighlightingBuilder;
extern const base::Feature kIPHSharedHighlightingReceiverFeature;
extern const base::Feature kIPHSharingHubWebnotesStylizeFeature;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
extern const base::Feature kIPHBottomToolbarTipFeature;
extern const base::Feature kIPHLongPressToolbarTipFeature;
extern const base::Feature kIPHNewTabTipFeature;
extern const base::Feature kIPHNewIncognitoTabTipFeature;
extern const base::Feature kIPHBadgedReadingListFeature;
extern const base::Feature kIPHReadingListMessagesFeature;
extern const base::Feature kIPHBadgedTranslateManualTriggerFeature;
extern const base::Feature kIPHDiscoverFeedHeaderFeature;
extern const base::Feature kIPHDefaultSiteViewFeature;
extern const base::Feature kIPHPasswordSuggestionsFeature;
extern const base::Feature kIPHFollowWhileBrowsingFeature;
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
extern const base::Feature kIPHAutofillVirtualCardSuggestionFeature;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
