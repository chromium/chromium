/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_constants.h"

#include "build/build_config.h"

namespace feature_engagement {

// Features used by the In-Product Help system.
const base::Feature kEnableAutomaticSnooze{"EnableAutomaticSnooze",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDemoMode{"IPH_DemoMode",
                                 base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHSnooze{"IPH_Snooze", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableIPH{"EnableIPH", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kUseClientConfigIPH{"UseClientConfigIPH",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Features used by various clients to show their In-Product Help messages.
const base::Feature kIPHDummyFeature{"IPH_Dummy",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
const base::Feature kIPHDesktopSharedHighlightingFeature{
    "IPH_DesktopSharedHighlighting", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDesktopTabGroupsNewGroupFeature{
    "IPH_DesktopTabGroupsNewGroup", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHFocusHelpBubbleScreenReaderPromoFeature{
    "IPH_FocusHelpBubbleScreenReaderPromo", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHGMCCastStartStopFeature{
    "IPH_GMCCastStartStop", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHLiveCaptionFeature{"IPH_LiveCaption",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHTabAudioMutingFeature{"IPH_TabAudioMuting",
                                              base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPasswordsAccountStorageFeature{
    "IPH_PasswordsAccountStorage", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHReadingListDiscoveryFeature{
    "IPH_ReadingListDiscovery", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHReadingListEntryPointFeature{
    "IPH_ReadingListEntryPoint", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHReadingListInSidePanelFeature{
    "IPH_ReadingListInSidePanel", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHReopenTabFeature{"IPH_ReopenTab",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHSideSearchFeature{"IPH_SideSearch",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTabSearchFeature{"IPH_TabSearch",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHWebUITabStripFeature{"IPH_WebUITabStrip",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDesktopSnoozeFeature{"IPH_DesktopSnoozeFeature",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDesktopPwaInstallFeature{
    "IPH_DesktopPwaInstall", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHProfileSwitchFeature{"IPH_ProfileSwitch",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHIntentChipFeature{"IPH_IntentChip",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
const base::Feature kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature{
    "IPH_AdaptiveButtonInTopToolbarCustomization_NewTab",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature{
    "IPH_AdaptiveButtonInTopToolbarCustomization_Share",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature
    kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature{
        "IPH_AdaptiveButtonInTopToolbarCustomization_VoiceSearch",
        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHAddToHomescreenMessageFeature{
    "IPH_AddToHomescreenMessage", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHAutoDarkOptOutFeature{"IPH_AutoDarkOptOut",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHAutoDarkUserEducationMessageFeature{
    "IPH_AutoDarkUserEducationMessage", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHAutoDarkUserEducationMessageOptInFeature{
    "IPH_AutoDarkUserEducationMessageOptIn", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHContextualPageActionsPriceTrackingFeature{
    "IPH_ContextualPageActions_PriceTracking",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHCrowFeature{"IPH_Crow",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDataSaverDetailFeature{
    "IPH_DataSaverDetail", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHDataSaverMilestonePromoFeature{
    "IPH_DataSaverMilestonePromo", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDataSaverPreviewFeature{
    "IPH_DataSaverPreview", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHDownloadHomeFeature{"IPH_DownloadHome",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHDownloadIndicatorFeature{
    "IPH_DownloadIndicator", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadPageFeature{"IPH_DownloadPage",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHDownloadPageScreenshotFeature{
    "IPH_DownloadPageScreenshot", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeHomeExpandFeature{
    "IPH_ChromeHomeExpand", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeHomePullToRefreshFeature{
    "IPH_ChromeHomePullToRefresh", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchTranslationEnableFeature{
    "IPH_ContextualSearchTranslationEnable", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchWebSearchFeature{
    "IPH_ContextualSearchWebSearch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchPromoteTapFeature{
    "IPH_ContextualSearchPromoteTap", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchPromotePanelOpenFeature{
    "IPH_ContextualSearchPromotePanelOpen", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchOptInFeature{
    "IPH_ContextualSearchOptIn", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchTappedButShouldLongpressFeature{
    "IPH_ContextualSearchTappedButShouldLongpress",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchInPanelHelpFeature{
    "IPH_ContextualSearchInPanelHelp", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadSettingsFeature{
    "IPH_DownloadSettings", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadInfoBarDownloadContinuingFeature{
    "IPH_DownloadInfoBarDownloadContinuing", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadInfoBarDownloadsAreFasterFeature{
    "IPH_DownloadInfoBarDownloadsAreFaster", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHQuietNotificationPromptsFeature{
    "IPH_QuietNotificationPrompts", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHReadLaterContextMenuFeature{
    "IPH_ReadLaterContextMenu", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHReadLaterAppMenuBookmarkThisPageFeature{
    "IPH_ReadLaterAppMenuBookmarkThisPage", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHReadLaterAppMenuBookmarksFeature{
    "IPH_ReadLaterAppMenuBookmarks", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHReadLaterBottomSheetFeature{
    "IPH_ReadLaterBottomSheet", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHShoppingListSaveFlowFeature{
    "IPH_ShoppingListSaveFlow", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHEphemeralTabFeature{"IPH_EphemeralTab",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature
    kIPHFeatureNotificationGuideDefaultBrowserNotificationShownFeature{
        "IPH_FeatureNotificationGuideDefaultBrowserNotificationShown",
        base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeatureNotificationGuideSignInNotificationShownFeature{
    "IPH_FeatureNotificationGuideSignInNotificationShown",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature
    kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature{
        "IPH_FeatureNotificationGuideIncognitoTabNotificationShown",
        base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature
    kIPHFeatureNotificationGuideNTPSuggestionCardNotificationShownFeature{
        "IPH_FeatureNotificationGuideNTPSuggestionCardNotificationShown",
        base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature
    kIPHFeatureNotificationGuideVoiceSearchNotificationShownFeature{
        "IPH_FeatureNotificationGuideVoiceSearchNotificationShown",
        base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeatureNotificationGuideDefaultBrowserPromoFeature{
    "IPH_FeatureNotificationGuideDefaultBrowserPromo",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeatureNotificationGuideSignInHelpBubbleFeature{
    "IPH_FeatureNotificationGuideSignInHelpBubble",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeatureNotificationGuideIncognitoTabHelpBubbleFeature{
    "IPH_FeatureNotificationGuideIncognitoTabHelpBubble",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature
    kIPHFeatureNotificationGuideNTPSuggestionCardHelpBubbleFeature{
        "IPH_FeatureNotificationGuideNTPSuggestionCardHelpBubble",
        base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeatureNotificationGuideVoiceSearchHelpBubbleFeature{
    "IPH_FeatureNotificationGuideVoiceSearchHelpBubble",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeatureNotificationGuideIncognitoTabUsedFeature{
    "IPH_FeatureNotificationGuideIncognitoTabUsed",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeatureNotificationGuideVoiceSearchUsedFeature{
    "IPH_FeatureNotificationGuideVoiceSearchUsed",
    base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeedCardMenuFeature{"IPH_FeedCardMenu",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHGenericAlwaysTriggerHelpUiFeature{
    "IPH_GenericAlwaysTriggerHelpUiFeature", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHIdentityDiscFeature{"IPH_IdentityDisc",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHInstanceSwitcherFeature{
    "IPH_InstanceSwitcher", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryAddressFillingFeature{
    "IPH_KeyboardAccessoryAddressFilling", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryBarSwipingFeature{
    "IPH_KeyboardAccessoryBarSwiping", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryPasswordFillingFeature{
    "IPH_KeyboardAccessoryPasswordFilling", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryPaymentFillingFeature{
    "IPH_KeyboardAccessoryPaymentFilling", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryPaymentOfferFeature{
    "IPH_KeyboardAccessoryPaymentOffer", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHLowUserEngagementDetectorFeature{
    "IPH_LowUserEngagementDetector", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHNewTabPageHomeButtonFeature{
    "IPH_NewTabPageHomeButton", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHMicToolbarFeature{"IPH_MicToolbar",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPageInfoFeature{"IPH_PageInfo",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPageInfoStoreInfoFeature{
    "IPH_PageInfoStoreInfo", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPreviewsOmniboxUIFeature{
    "IPH_PreviewsOmniboxUI", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHPriceDropNTPFeature{"IPH_PriceDropNTP",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHShoppingListMenuItemFeature{
    "IPH_ShoppingListMenuItem", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHTabGroupsQuicklyComparePagesFeature{
    "IPH_TabGroupsQuicklyComparePages", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTabGroupsTapToSeeAnotherTabFeature{
    "IPH_TabGroupsTapToSeeAnotherTab", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTabGroupsYourTabsAreTogetherFeature{
    "IPH_TabGroupsYourTabsTogether", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTabGroupsDragAndDropFeature{
    "IPH_TabGroupsDragAndDrop", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTabSwitcherButtonFeature{
    "IPH_TabSwitcherButton", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHTranslateMenuButtonFeature{
    "IPH_TranslateMenuButton", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHVideoTutorialNTPChromeIntroFeature{
    "IPH_VideoTutorial_NTP_ChromeIntro", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHVideoTutorialNTPDownloadFeature{
    "IPH_VideoTutorial_NTP_Download", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHVideoTutorialNTPSearchFeature{
    "IPH_VideoTutorial_NTP_Search", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHVideoTutorialNTPVoiceSearchFeature{
    "IPH_VideoTutorial_NTP_VoiceSearch", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHVideoTutorialNTPSummaryFeature{
    "IPH_VideoTutorial_NTP_Summary", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHVideoTutorialTryNowFeature{
    "IPH_VideoTutorial_TryNow", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHExploreSitesTileFeature{
    "IPH_ExploreSitesTile", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHFeedHeaderMenuFeature{"IPH_FeedHeaderMenu",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHWebFeedAwarenessFeature{
    "IPH_WebFeedAwareness", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHFeedSwipeRefresh{"IPH_FeedSwipeRefresh",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHChromeReengagementNotification1Feature{
    "IPH_ChromeReengagementNotification1", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeReengagementNotification2Feature{
    "IPH_ChromeReengagementNotification2", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeReengagementNotification3Feature{
    "IPH_ChromeReengagementNotification3", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPwaInstallAvailableFeature{
    "IPH_PwaInstallAvailableFeature", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHShareScreenshotFeature{
    "IPH_ShareScreenshot", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHSharingHubLinkToggleFeature{
    "IPH_SharingHubLinkToggle", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHWebFeedFollowFeature{"IPH_WebFeedFollow",
                                             base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHWebFeedPostFollowDialogFeature{
    "IPH_WebFeedPostFollowDialog", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHSharedHighlightingBuilder{
    "IPH_SharedHighlightingBuilder", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHSharedHighlightingReceiverFeature{
    "IPH_SharedHighlightingReceiver", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHSharingHubWebnotesStylizeFeature{
    "IPH_SharingHubWebnotesStylize", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
const base::Feature kIPHBottomToolbarTipFeature{
    "IPH_BottomToolbarTip", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHLongPressToolbarTipFeature{
    "IPH_LongPressToolbarTip", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHNewTabTipFeature{"IPH_NewTabTip",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHNewIncognitoTabTipFeature{
    "IPH_NewIncognitoTabTip", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHBadgedReadingListFeature{
    "IPH_BadgedReadingList", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHReadingListMessagesFeature{
    "IPH_ReadingListMessages", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHBadgedTranslateManualTriggerFeature{
    "IPH_BadgedTranslateManualTrigger", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDiscoverFeedHeaderFeature{
    "IPH_DiscoverFeedHeaderMenu", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDefaultSiteViewFeature{
    "IPH_DefaultSiteView", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPasswordSuggestionsFeature{
    "IPH_PasswordSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHFollowWhileBrowsingFeature{
    "IPH_FollowWhileBrowsing", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
const base::Feature kIPHAutofillVirtualCardSuggestionFeature{
    "IPH_AutofillVirtualCardSuggestion", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

}  // namespace feature_engagement
