// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/flags_ui/feature_entry.h"

namespace feature_engagement {
using FeatureVector = std::vector<const base::Feature*>;

// The param name for the FeatureVariation configuration, which is used by
// chrome://flags to set the variable name for the selected feature. The Tracker
// backend will then read this to figure out which feature (if any) was selected
// by the end user.
extern const char kIPHDemoModeFeatureChoiceParam[];

namespace {

// Defines a const flags_ui::FeatureEntry::FeatureParam for the given
// base::Feature. The constant name will be on the form
// kFooFeature --> kFooFeatureVariation. The |feature_name| argument must
// match the base::Feature::name member of the |base_feature|.
// This is intended to be used with VARIATION_ENTRY below to be able to insert
// it into an array of flags_ui::FeatureEntry::FeatureVariation.
#define DEFINE_VARIATION_PARAM(base_feature, feature_name)                     \
  constexpr flags_ui::FeatureEntry::FeatureParam base_feature##Variation[] = { \
      {kIPHDemoModeFeatureChoiceParam, feature_name}}

// Defines a single flags_ui::FeatureEntry::FeatureVariation entry, fully
// enclosed. This is intended to be used with the declaration of
// |kIPHDemoModeChoiceVariations| below.
#define VARIATION_ENTRY(base_feature)                                \
  {                                                                  \
    base_feature##Variation[0].param_value, base_feature##Variation, \
        std::size(base_feature##Variation), nullptr                  \
  }

// Defines a flags_ui::FeatureEntry::FeatureParam for each feature.
DEFINE_VARIATION_PARAM(kIPHDummyFeature, "IPH_Dummy");
#if BUILDFLAG(IS_ANDROID)
DEFINE_VARIATION_PARAM(kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
                       "IPH_AdaptiveButtonInTopToolbarCustomization_NewTab");
DEFINE_VARIATION_PARAM(kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature,
                       "IPH_AdaptiveButtonInTopToolbarCustomization_Share");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_VoiceSearch");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationTranslateFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_Translate");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationAddToBookmarksFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_AddToBookmarks");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationReadAloudFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_ReadAloud");
DEFINE_VARIATION_PARAM(kIPHAddToHomescreenMessageFeature,
                       "IPH_AddToHomescreenMessage");
DEFINE_VARIATION_PARAM(kIPHAutoDarkOptOutFeature, "IPH_AutoDarkOptOut");
DEFINE_VARIATION_PARAM(kIPHAutoDarkUserEducationMessageFeature,
                       "IPH_AutoDarkUserEducationMessage");
DEFINE_VARIATION_PARAM(kIPHAutoDarkUserEducationMessageOptInFeature,
                       "IPH_AutoDarkUserEducationMessageOptIn");
DEFINE_VARIATION_PARAM(kIPHContextualPageActionsQuietVariantFeature,
                       "IPH_ContextualPageActions_QuietVariant");
DEFINE_VARIATION_PARAM(kIPHContextualPageActionsActionChipFeature,
                       "IPH_ContextualPageActions_ActionChip");
DEFINE_VARIATION_PARAM(kIPHDataSaverDetailFeature, "IPH_DataSaverDetail");
DEFINE_VARIATION_PARAM(kIPHDataSaverMilestonePromoFeature,
                       "IPH_DataSaverMilestonePromo");
DEFINE_VARIATION_PARAM(kIPHDataSaverPreviewFeature, "IPH_DataSaverPreview");
DEFINE_VARIATION_PARAM(kIPHDownloadHomeFeature, "IPH_DownloadHome");
DEFINE_VARIATION_PARAM(kIPHDownloadIndicatorFeature, "IPH_DownloadIndicator");
DEFINE_VARIATION_PARAM(kIPHDownloadPageFeature, "IPH_DownloadPage");
DEFINE_VARIATION_PARAM(kIPHDownloadPageScreenshotFeature,
                       "IPH_DownloadPageScreenshot");
DEFINE_VARIATION_PARAM(kIPHChromeHomeExpandFeature, "IPH_ChromeHomeExpand");
DEFINE_VARIATION_PARAM(kIPHChromeHomePullToRefreshFeature,
                       "IPH_ChromeHomePullToRefresh");
DEFINE_VARIATION_PARAM(kIPHChromeReengagementNotification1Feature,
                       "IPH_ChromeReengagementNotification1");
DEFINE_VARIATION_PARAM(kIPHChromeReengagementNotification2Feature,
                       "IPH_ChromeReengagementNotification2");
DEFINE_VARIATION_PARAM(kIPHChromeReengagementNotification3Feature,
                       "IPH_ChromeReengagementNotification3");
DEFINE_VARIATION_PARAM(kIPHDownloadSettingsFeature, "IPH_DownloadSettings");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadContinuingFeature,
                       "IPH_DownloadInfoBarDownloadContinuing");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadsAreFasterFeature,
                       "IPH_DownloadInfoBarDownloadsAreFaster");
DEFINE_VARIATION_PARAM(kIPHEphemeralTabFeature, "IPH_EphemeralTab");
DEFINE_VARIATION_PARAM(
    kIPHFeatureNotificationGuideDefaultBrowserNotificationShownFeature,
    "IPH_FeatureNotificationGuideDefaultBrowserNotificationShown");
DEFINE_VARIATION_PARAM(
    kIPHFeatureNotificationGuideSignInNotificationShownFeature,
    "IPH_FeatureNotificationGuideSignInNotificationShown");
DEFINE_VARIATION_PARAM(
    kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature,
    "IPH_FeatureNotificationGuideIncognitoTabNotificationShown");
DEFINE_VARIATION_PARAM(
    kIPHFeatureNotificationGuideNTPSuggestionCardNotificationShownFeature,
    "IPH_FeatureNotificationGuideNTPSuggestionCardNotificationShown");
DEFINE_VARIATION_PARAM(
    kIPHFeatureNotificationGuideVoiceSearchNotificationShownFeature,
    "IPH_FeatureNotificationGuideVoiceSearchNotificationShown");
DEFINE_VARIATION_PARAM(kIPHFeatureNotificationGuideDefaultBrowserPromoFeature,
                       "IPH_FeatureNotificationGuideDefaultBrowserPromo");
DEFINE_VARIATION_PARAM(kIPHFeatureNotificationGuideSignInHelpBubbleFeature,
                       "IPH_FeatureNotificationGuideSignInHelpBubble");
DEFINE_VARIATION_PARAM(
    kIPHFeatureNotificationGuideIncognitoTabHelpBubbleFeature,
    "IPH_FeatureNotificationGuideIncognitoTabHelpBubble");
DEFINE_VARIATION_PARAM(
    kIPHFeatureNotificationGuideNTPSuggestionCardHelpBubbleFeature,
    "IPH_FeatureNotificationGuideNTPSuggestionCardHelpBubble");
DEFINE_VARIATION_PARAM(kIPHFeatureNotificationGuideVoiceSearchHelpBubbleFeature,
                       "IPH_FeatureNotificationGuideVoiceSearchHelpBubble");
DEFINE_VARIATION_PARAM(kIPHFeedCardMenuFeature, "IPH_FeedCardMenu");
DEFINE_VARIATION_PARAM(kIPHGenericAlwaysTriggerHelpUiFeature,
                       "IPH_GenericAlwaysTriggerHelpUiFeature");
DEFINE_VARIATION_PARAM(kIPHIdentityDiscFeature, "IPH_IdentityDisc");
DEFINE_VARIATION_PARAM(kIPHInstanceSwitcherFeature, "IPH_InstanceSwitcher");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryAddressFillingFeature,
                       "IPH_KeyboardAccessoryAddressFilling");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryBarSwipingFeature,
                       "IPH_KeyboardAccessoryBarSwiping");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryPasswordFillingFeature,
                       "IPH_KeyboardAccessoryPasswordFilling");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryPaymentFillingFeature,
                       "IPH_KeyboardAccessoryPaymentFilling");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryPaymentOfferFeature,
                       "IPH_KeyboardAccessoryPaymentOffer");
DEFINE_VARIATION_PARAM(kIPHMicToolbarFeature, "IPH_MicToolbar");
DEFINE_VARIATION_PARAM(kIPHNewTabPageButtonFeature, "IPH_NewTabPageHomeButton");
DEFINE_VARIATION_PARAM(kIPHPageInfoFeature, "IPH_PageInfo");
DEFINE_VARIATION_PARAM(kIPHPageInfoStoreInfoFeature, "IPH_PageInfoStoreInfo");
DEFINE_VARIATION_PARAM(kIPHPageZoomFeature, "IPH_PageZoom");
DEFINE_VARIATION_PARAM(kIPHPreviewsOmniboxUIFeature, "IPH_PreviewsOmniboxUI");
DEFINE_VARIATION_PARAM(kIPHPriceDropNTPFeature, "IPH_PriceDropNTP");
DEFINE_VARIATION_PARAM(kIPHPwaInstallAvailableFeature,
                       "IPH_PwaInstallAvailableFeature");
DEFINE_VARIATION_PARAM(kIPHQuietNotificationPromptsFeature,
                       "IPH_QuietNotificationPrompts");
DEFINE_VARIATION_PARAM(kIPHReadLaterContextMenuFeature,
                       "IPH_ReadLaterContextMenu");
DEFINE_VARIATION_PARAM(kIPHReadLaterAppMenuBookmarkThisPageFeature,
                       "IPH_ReadLaterAppMenuBookmarkThisPage");
DEFINE_VARIATION_PARAM(kIPHReadLaterAppMenuBookmarksFeature,
                       "IPH_ReadLaterAppMenuBookmarks");
DEFINE_VARIATION_PARAM(kIPHReadLaterBottomSheetFeature,
                       "IPH_ReadLaterBottomSheet");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteAppMenuFeature,
                       "IPH_RequestDesktopSiteAppMenu");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteDefaultOnFeature,
                       "IPH_RequestDesktopSiteDefaultOn");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteOptInFeature,
                       "IPH_RequestDesktopSiteOptIn");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteExceptionsGenericFeature,
                       "IPH_RequestDesktopSiteExceptionsGeneric");
DEFINE_VARIATION_PARAM(kIPHShoppingListMenuItemFeature,
                       "IPH_ShoppingListMenuItem");
DEFINE_VARIATION_PARAM(kIPHShoppingListSaveFlowFeature,
                       "IPH_ShoppingListSaveFlow");
DEFINE_VARIATION_PARAM(kIPHTabGroupsQuicklyComparePagesFeature,
                       "IPH_TabGroupsQuicklyComparePages");
DEFINE_VARIATION_PARAM(kIPHTabGroupsTapToSeeAnotherTabFeature,
                       "IPH_TabGroupsTapToSeeAnotherTab");
DEFINE_VARIATION_PARAM(kIPHTabGroupsYourTabsAreTogetherFeature,
                       "IPH_TabGroupsYourTabsTogether");
DEFINE_VARIATION_PARAM(kIPHTabGroupsDragAndDropFeature,
                       "IPH_TabGroupsDragAndDrop");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherButtonFeature, "IPH_TabSwitcherButton");
DEFINE_VARIATION_PARAM(kIPHTranslateMenuButtonFeature,
                       "IPH_TranslateMenuButton");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPChromeIntroFeature,
                       "IPH_VideoTutorial_NTP_ChromeIntro");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPDownloadFeature,
                       "IPH_VideoTutorial_NTP_Download");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPSearchFeature,
                       "IPH_VideoTutorial_NTP_Search");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPVoiceSearchFeature,
                       "IPH_VideoTutorial_NTP_VoiceSearch");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPSummaryFeature,
                       "IPH_VideoTutorial_NTP_Summary");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialTryNowFeature,
                       "IPH_VideoTutorial_TryNow");
DEFINE_VARIATION_PARAM(kIPHExploreSitesTileFeature, "IPH_ExploreSitesTile");
DEFINE_VARIATION_PARAM(kIPHFeedHeaderMenuFeature, "IPH_FeedHeaderMenu");
DEFINE_VARIATION_PARAM(kIPHWebFeedAwarenessFeature, "IPH_WebFeedAwareness");
DEFINE_VARIATION_PARAM(kIPHFeedSwipeRefresh, "IPH_FeedSwipeRefresh");
DEFINE_VARIATION_PARAM(kIPHShareScreenshotFeature, "IPH_ShareScreenshot");
DEFINE_VARIATION_PARAM(kIPHSharingHubLinkToggleFeature,
                       "IPH_SharingHubLinkToggle");
DEFINE_VARIATION_PARAM(kIPHWebFeedFollowFeature, "IPH_WebFeedFollow");
DEFINE_VARIATION_PARAM(kIPHWebFeedPostFollowDialogFeature,
                       "IPH_WebFeedPostFollowDialog");
DEFINE_VARIATION_PARAM(kIPHSharedHighlightingBuilder,
                       "IPH_SharedHighlightingBuilder");
DEFINE_VARIATION_PARAM(kIPHSharedHighlightingReceiverFeature,
                       "IPH_SharedHighlightingReceiver");
DEFINE_VARIATION_PARAM(kIPHSharingHubWebnotesStylizeFeature,
                       "IPH_SharingHubWebnotesStylize");
DEFINE_VARIATION_PARAM(kIPHRestoreTabsOnFREFeature, "IPH_RestoreTabsOnFRE");
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_IOS)
DEFINE_VARIATION_PARAM(kIPHBottomToolbarTipFeature, "IPH_BottomToolbarTip");
DEFINE_VARIATION_PARAM(kIPHLongPressToolbarTipFeature,
                       "IPH_LongPressToolbarTip");
DEFINE_VARIATION_PARAM(kIPHNewIncognitoTabTipFeature, "IPH_NewIncognitoTabTip");
DEFINE_VARIATION_PARAM(kIPHBadgedReadingListFeature, "IPH_BadgedReadingList");
DEFINE_VARIATION_PARAM(kIPHWhatsNewFeature, "IPH_WhatsNew");
DEFINE_VARIATION_PARAM(kIPHReadingListMessagesFeature,
                       "IPH_ReadingListMessages");
DEFINE_VARIATION_PARAM(kIPHBadgedTranslateManualTriggerFeature,
                       "IPH_BadgedTranslateManualTrigger");
DEFINE_VARIATION_PARAM(kIPHDiscoverFeedHeaderFeature,
                       "IPH_DiscoverFeedHeaderMenu");
DEFINE_VARIATION_PARAM(kIPHDefaultSiteViewFeature, "IPH_DefaultSiteView");
DEFINE_VARIATION_PARAM(kIPHFollowWhileBrowsingFeature,
                       "IPH_FollowWhileBrowsing");
DEFINE_VARIATION_PARAM(kIPHPriceNotificationsWhileBrowsingFeature,
                       "IPHPriceNotificationsWhileBrowsing");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserBadgeEligibilityFeature,
                       "IPH_iOSDefaultBrowserBadgeEligibility");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserOverflowMenuBadgeFeature,
                       "IPH_iOSDefaultBrowserOverflowMenuBadge");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserSettingsBadgeFeature,
                       "IPH_iOSDefaultBrowserSettingsBadge");
DEFINE_VARIATION_PARAM(kIPHiOSPromoAppStoreFeature, "IPH_iOSPromoAppStore");
DEFINE_VARIATION_PARAM(kIPHiOSPromoWhatsNewFeature, "IPH_iOSPromoWhatsNew");
DEFINE_VARIATION_PARAM(kIPHiOSPromoPostRestoreFeature,
                       "IPH_iOSPromoPostRestore");
DEFINE_VARIATION_PARAM(kIPHiOSPromoCredentialProviderExtensionFeature,
                       "IPH_iOSPromoCredentialProviderExtension");
DEFINE_VARIATION_PARAM(kIPHiOSPromoDefaultBrowserFeature,
                       "IPH_iOSPromoDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSNewTabToolbarItemFeature,
                       "IPH_iOSNewTabToolbarItemFeature");
DEFINE_VARIATION_PARAM(kIPHiOSTabGridToolbarItemFeature,
                       "IPH_iOSTabGridToolbarItemFeature");
DEFINE_VARIATION_PARAM(kIPHiOSHistoryOnOverflowMenuFeature,
                       "IPH_iOSHistoryOnOverflowMenuFeature");
DEFINE_VARIATION_PARAM(kIPHiOSShareToolbarItemFeature,
                       "IPH_iOSShareToolbarItemFeature");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserVideoPromoTriggerFeature,
                       "IPH_iOSDefaultBrowserVideoPromoTriggerFeature");
DEFINE_VARIATION_PARAM(kIPHiOSPromoPostRestoreDefaultBrowserFeature,
                       "IPH_iOSPromoPostDefaultBrowserRestore");
DEFINE_VARIATION_PARAM(kIPHiOSPromoPasswordManagerWidgetFeature,
                       "IPH_iOSPromoPasswordManagerWidget");
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
DEFINE_VARIATION_PARAM(kIPHAutofillFeedbackNewBadgeFeature,
                       "IPH_AutofillFeedbackNewBadge");
DEFINE_VARIATION_PARAM(kIPHBatterySaverModeFeature, "IPH_BatterySaverMode");
DEFINE_VARIATION_PARAM(kIPHCompanionSidePanelFeature, "IPH_CompanionSidePanel");
DEFINE_VARIATION_PARAM(kIPHCompanionSidePanelRegionSearchFeature,
                       "IPH_CompanionSidePanelRegionSearch");
DEFINE_VARIATION_PARAM(kIPHDesktopCustomizeChromeFeature,
                       "IPH_DesktopCustomizeChrome");
DEFINE_VARIATION_PARAM(kIPHDesktopCustomizeChromeRefreshFeature,
                       "IPH_DesktopCustomizeChromeRefresh");
DEFINE_VARIATION_PARAM(kIPHDesktopTabGroupsNewGroupFeature,
                       "IPH_DesktopTabGroupsNewGroup");
DEFINE_VARIATION_PARAM(kIPHDownloadToolbarButtonFeature,
                       "IPH_DownloadToolbarButton");
DEFINE_VARIATION_PARAM(kIPHExtensionsMenuFeature, "IPH_ExtensionsMenu");
DEFINE_VARIATION_PARAM(kIPHFocusModeFeature, "IPH_FocusMode");
DEFINE_VARIATION_PARAM(kIPHExtensionsRequestAccessButtonFeature,
                       "IPH_ExtensionsRequestAccessButton");
DEFINE_VARIATION_PARAM(kIPHGlobalMediaControls, "IPH_GlobalMediaControls");
DEFINE_VARIATION_PARAM(kIPHGMCCastStartStopFeature, "IPH_GMCCastStartStop");
DEFINE_VARIATION_PARAM(kIPHHighEfficiencyModeFeature, "IPH_HighEfficiencyMode");
DEFINE_VARIATION_PARAM(kIPHLiveCaption, "IPH_LiveCaption");
DEFINE_VARIATION_PARAM(kIPHPasswordsAccountStorageFeature,
                       "IPH_PasswordsAccountStorage");
DEFINE_VARIATION_PARAM(kIPHPasswordsManagementBubbleAfterSaveFeature,
                       "IPH_PasswordsManagementBubbleAfterSave");
DEFINE_VARIATION_PARAM(kIPHPasswordsManagementBubbleDuringSigninFeature,
                       "IPH_PasswordsManagementBubbleDuringSignin");
DEFINE_VARIATION_PARAM(kIPHPasswordsWebAppProfileSwitchFeature,
                       "IPH_PasswordsWebAppProfileSwitch");
DEFINE_VARIATION_PARAM(kIPHPasswordManagerShortcutFeature,
                       "IPH_PasswordManagerShortcut");
DEFINE_VARIATION_PARAM(kIPHPerformanceNewBadgeFeature,
                       "IPH_PerformanceNewBadge");
DEFINE_VARIATION_PARAM(kIPHPowerBookmarksSidePanelFeature,
                       "IPH_PowerBookmarksSidePanel");
DEFINE_VARIATION_PARAM(kIPHPriceInsightsPageActionIconLabelFeature,
                       "IPH_PriceInsightsPageActionIconLabelFeature");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingChipFeature,
                       "IPH_PriceTrackingChipFeature");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingEmailConsentFeature,
                       "IPH_PriceTrackingEmailConsentFeature");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingPageActionIconLabelFeature,
                       "IPH_PriceTrackingPageActionIconLabelFeature");
DEFINE_VARIATION_PARAM(kIPHReadingListDiscoveryFeature,
                       "IPH_ReadingListDiscovery");
DEFINE_VARIATION_PARAM(kIPHReadingListEntryPointFeature,
                       "IPH_ReadingListEntryPoint");
DEFINE_VARIATION_PARAM(kIPHReadingListInSidePanelFeature,
                       "IPH_ReadingListInSidePanel");
DEFINE_VARIATION_PARAM(kIPHReadingModeSidePanelFeature,
                       "IPH_ReadingModeSidePanel");
DEFINE_VARIATION_PARAM(kIPHShoppingCollectionFeature,
                       "IPH_ShoppingCollectionFeature");
DEFINE_VARIATION_PARAM(kIPHSideSearchAutoTriggeringFeature,
                       "IPH_SideSearchAutoTriggering");
DEFINE_VARIATION_PARAM(kIPHSideSearchFeature, "IPH_SideSearch");
DEFINE_VARIATION_PARAM(kIPHSideSearchPageActionLabelFeature,
                       "IPH_SideSearchPageActionLabel");
DEFINE_VARIATION_PARAM(kIPHTabAudioMutingFeature, "IPH_TabAudioMuting");
DEFINE_VARIATION_PARAM(kIPHTabSearchFeature, "IPH_TabSearch");
DEFINE_VARIATION_PARAM(kIPHWebUITabStripFeature, "IPH_WebUITabStrip");
DEFINE_VARIATION_PARAM(kIPHDesktopPwaInstallFeature, "IPH_DesktopPwaInstall");
DEFINE_VARIATION_PARAM(kIPHProfileSwitchFeature, "IPH_ProfileSwitch");
DEFINE_VARIATION_PARAM(kIPHDesktopSharedHighlightingFeature,
                       "IPH_DesktopSharedHighlighting");
DEFINE_VARIATION_PARAM(kIPHWebUiHelpBubbleTestFeature,
                       "IPH_WebUiHelpBubbleTest");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingInSidePanelFeature,
                       "IPH_PriceTrackingInSidePanel");
DEFINE_VARIATION_PARAM(kIPHBackNavigationMenuFeature, "IPH_BackNavigationMenu");
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
DEFINE_VARIATION_PARAM(kIPHAutofillExternalAccountProfileSuggestionFeature,
                       "IPH_AutofillExternalAccountProfileSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillVirtualCardCVCSuggestionFeature,
                       "IPH_AutofillVirtualCardCVCSuggestionFeature");
DEFINE_VARIATION_PARAM(kIPHAutofillVirtualCardSuggestionFeature,
                       "IPH_AutofillVirtualCardSuggestion");
DEFINE_VARIATION_PARAM(kIPHCookieControlsFeature, "IPH_CookieControls");
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS_ASH)
DEFINE_VARIATION_PARAM(kIPHGoogleOneOfferNotificationFeature,
                       "IPH_GoogleOneOfferNotification");
DEFINE_VARIATION_PARAM(kIPHLauncherSearchHelpUiFeature,
                       "IPH_LauncherSearchHelpUi");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedOneFeature,
                       "IPH_ScalableIphTimerBasedOne");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedTwoFeature,
                       "IPH_ScalableIphTimerBasedTwo");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedThreeFeature,
                       "IPH_ScalableIphTimerBasedThree");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedFourFeature,
                       "IPH_ScalableIphTimerBasedFour");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedFiveFeature,
                       "IPH_ScalableIphTimerBasedFive");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedSixFeature,
                       "IPH_ScalableIphTimerBasedSix");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedSevenFeature,
                       "IPH_ScalableIphTimerBasedSeven");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedEightFeature,
                       "IPH_ScalableIphTimerBasedEight");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedNineFeature,
                       "IPH_ScalableIphTimerBasedNine");
DEFINE_VARIATION_PARAM(kIPHScalableIphTimerBasedTenFeature,
                       "IPH_ScalableIphTimerBasedTen");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedOneFeature,
                       "IPH_ScalableIphUnlockedBasedOne");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedTwoFeature,
                       "IPH_ScalableIphUnlockedBasedTwo");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedThreeFeature,
                       "IPH_ScalableIphUnlockedBasedThree");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedFourFeature,
                       "IPH_ScalableIphUnlockedBasedFour");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedFiveFeature,
                       "IPH_ScalableIphUnlockedBasedFive");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedSixFeature,
                       "IPH_ScalableIphUnlockedBasedSix");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedSevenFeature,
                       "IPH_ScalableIphUnlockedBasedSeven");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedEightFeature,
                       "IPH_ScalableIphUnlockedBasedEight");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedNineFeature,
                       "IPH_ScalableIphUnlockedBasedNine");
DEFINE_VARIATION_PARAM(kIPHScalableIphUnlockedBasedTenFeature,
                       "IPH_ScalableIphUnlockedBasedTen");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedNudgeFeature,
                       "IPH_ScalableIphHelpAppBasedNudge");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedOneFeature,
                       "IPH_ScalableIphHelpAppBasedOne");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedTwoFeature,
                       "IPH_ScalableIphHelpAppBasedTwo");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedThreeFeature,
                       "IPH_ScalableIphHelpAppBasedThree");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedFourFeature,
                       "IPH_ScalableIphHelpAppBasedFour");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedFiveFeature,
                       "IPH_ScalableIphHelpAppBasedFive");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedSixFeature,
                       "IPH_ScalableIphHelpAppBasedSix");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedSevenFeature,
                       "IPH_ScalableIphHelpAppBasedSeven");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedEightFeature,
                       "IPH_ScalableIphHelpAppBasedEight");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedNineFeature,
                       "IPH_ScalableIphHelpAppBasedNine");
DEFINE_VARIATION_PARAM(kIPHScalableIphHelpAppBasedTenFeature,
                       "IPH_ScalableIphHelpAppBasedTen");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
DEFINE_VARIATION_PARAM(kIPHiOSPasswordPromoDesktopFeature,
                       "IPH_iOSPasswordPromoDesktop");
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

// Defines the array of which features should be listed in the chrome://flags
// UI to be able to select them alone for demo-mode. The features listed here
// are possible to enable on their own in demo mode.
constexpr flags_ui::FeatureEntry::FeatureVariation
    kIPHDemoModeChoiceVariations[] = {
#if BUILDFLAG(IS_ANDROID)
        VARIATION_ENTRY(
            kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature),
        VARIATION_ENTRY(
            kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature),
        VARIATION_ENTRY(
            kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature),
        VARIATION_ENTRY(
            kIPHAdaptiveButtonInTopToolbarCustomizationTranslateFeature),
        VARIATION_ENTRY(
            kIPHAdaptiveButtonInTopToolbarCustomizationAddToBookmarksFeature),
        VARIATION_ENTRY(
            kIPHAdaptiveButtonInTopToolbarCustomizationReadAloudFeature),
        VARIATION_ENTRY(kIPHAddToHomescreenMessageFeature),
        VARIATION_ENTRY(kIPHAutoDarkOptOutFeature),
        VARIATION_ENTRY(kIPHAutoDarkUserEducationMessageFeature),
        VARIATION_ENTRY(kIPHAutoDarkUserEducationMessageOptInFeature),
        VARIATION_ENTRY(kIPHContextualPageActionsQuietVariantFeature),
        VARIATION_ENTRY(kIPHContextualPageActionsActionChipFeature),
        VARIATION_ENTRY(kIPHDataSaverDetailFeature),
        VARIATION_ENTRY(kIPHDataSaverMilestonePromoFeature),
        VARIATION_ENTRY(kIPHDataSaverPreviewFeature),
        VARIATION_ENTRY(kIPHDownloadHomeFeature),
        VARIATION_ENTRY(kIPHDownloadIndicatorFeature),
        VARIATION_ENTRY(kIPHDownloadPageFeature),
        VARIATION_ENTRY(kIPHDownloadPageScreenshotFeature),
        VARIATION_ENTRY(kIPHChromeHomeExpandFeature),
        VARIATION_ENTRY(kIPHChromeHomePullToRefreshFeature),
        VARIATION_ENTRY(kIPHChromeReengagementNotification1Feature),
        VARIATION_ENTRY(kIPHChromeReengagementNotification2Feature),
        VARIATION_ENTRY(kIPHChromeReengagementNotification3Feature),
        VARIATION_ENTRY(kIPHDownloadSettingsFeature),
        VARIATION_ENTRY(kIPHDownloadInfoBarDownloadContinuingFeature),
        VARIATION_ENTRY(kIPHDownloadInfoBarDownloadsAreFasterFeature),
        VARIATION_ENTRY(kIPHEphemeralTabFeature),
        VARIATION_ENTRY(kIPHFeedCardMenuFeature),
        VARIATION_ENTRY(kIPHIdentityDiscFeature),
        VARIATION_ENTRY(kIPHInstanceSwitcherFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryAddressFillingFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryBarSwipingFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryPasswordFillingFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryPaymentFillingFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryPaymentOfferFeature),
        VARIATION_ENTRY(kIPHMicToolbarFeature),
        VARIATION_ENTRY(kIPHNewTabPageButtonFeature),
        VARIATION_ENTRY(kIPHPageInfoFeature),
        VARIATION_ENTRY(kIPHPageInfoStoreInfoFeature),
        VARIATION_ENTRY(kIPHPageZoomFeature),
        VARIATION_ENTRY(kIPHPreviewsOmniboxUIFeature),
        VARIATION_ENTRY(kIPHPriceDropNTPFeature),
        VARIATION_ENTRY(kIPHPwaInstallAvailableFeature),
        VARIATION_ENTRY(kIPHQuietNotificationPromptsFeature),
        VARIATION_ENTRY(kIPHReadLaterContextMenuFeature),
        VARIATION_ENTRY(kIPHReadLaterAppMenuBookmarkThisPageFeature),
        VARIATION_ENTRY(kIPHReadLaterAppMenuBookmarksFeature),
        VARIATION_ENTRY(kIPHReadLaterBottomSheetFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteAppMenuFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteDefaultOnFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteOptInFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteExceptionsGenericFeature),
        VARIATION_ENTRY(kIPHShoppingListMenuItemFeature),
        VARIATION_ENTRY(kIPHShoppingListSaveFlowFeature),
        VARIATION_ENTRY(kIPHTabGroupsQuicklyComparePagesFeature),
        VARIATION_ENTRY(kIPHTabGroupsTapToSeeAnotherTabFeature),
        VARIATION_ENTRY(kIPHTabGroupsYourTabsAreTogetherFeature),
        VARIATION_ENTRY(kIPHTabGroupsDragAndDropFeature),
        VARIATION_ENTRY(kIPHTabSwitcherButtonFeature),
        VARIATION_ENTRY(kIPHTranslateMenuButtonFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPChromeIntroFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPDownloadFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPSearchFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPVoiceSearchFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPSummaryFeature),
        VARIATION_ENTRY(kIPHVideoTutorialTryNowFeature),
        VARIATION_ENTRY(kIPHExploreSitesTileFeature),
        VARIATION_ENTRY(kIPHFeedHeaderMenuFeature),
        VARIATION_ENTRY(kIPHFeedSwipeRefresh),
        VARIATION_ENTRY(kIPHShareScreenshotFeature),
        VARIATION_ENTRY(kIPHSharingHubLinkToggleFeature),
        VARIATION_ENTRY(kIPHWebFeedAwarenessFeature),
        VARIATION_ENTRY(kIPHWebFeedFollowFeature),
        VARIATION_ENTRY(kIPHWebFeedPostFollowDialogFeature),
        VARIATION_ENTRY(kIPHSharedHighlightingBuilder),
        VARIATION_ENTRY(kIPHSharedHighlightingReceiverFeature),
        VARIATION_ENTRY(kIPHSharingHubWebnotesStylizeFeature),
        VARIATION_ENTRY(kIPHRestoreTabsOnFREFeature),
#elif BUILDFLAG(IS_IOS)
        VARIATION_ENTRY(kIPHBottomToolbarTipFeature),
        VARIATION_ENTRY(kIPHLongPressToolbarTipFeature),
        VARIATION_ENTRY(kIPHNewIncognitoTabTipFeature),
        VARIATION_ENTRY(kIPHBadgedReadingListFeature),
        VARIATION_ENTRY(kIPHReadingListMessagesFeature),
        VARIATION_ENTRY(kIPHWhatsNewFeature),
        VARIATION_ENTRY(kIPHBadgedTranslateManualTriggerFeature),
        VARIATION_ENTRY(kIPHDiscoverFeedHeaderFeature),
        VARIATION_ENTRY(kIPHDefaultSiteViewFeature),
        VARIATION_ENTRY(kIPHFollowWhileBrowsingFeature),
        VARIATION_ENTRY(kIPHPriceNotificationsWhileBrowsingFeature),
        VARIATION_ENTRY(kIPHiOSDefaultBrowserBadgeEligibilityFeature),
        VARIATION_ENTRY(kIPHiOSDefaultBrowserOverflowMenuBadgeFeature),
        VARIATION_ENTRY(kIPHiOSDefaultBrowserSettingsBadgeFeature),
        VARIATION_ENTRY(kIPHiOSPromoAppStoreFeature),
        VARIATION_ENTRY(kIPHiOSPromoWhatsNewFeature),
        VARIATION_ENTRY(kIPHiOSPromoPostRestoreFeature),
        VARIATION_ENTRY(kIPHiOSPromoCredentialProviderExtensionFeature),
        VARIATION_ENTRY(kIPHiOSPromoDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSNewTabToolbarItemFeature),
        VARIATION_ENTRY(kIPHiOSTabGridToolbarItemFeature),
        VARIATION_ENTRY(kIPHiOSHistoryOnOverflowMenuFeature),
        VARIATION_ENTRY(kIPHiOSShareToolbarItemFeature),
        VARIATION_ENTRY(kIPHiOSPromoPostRestoreDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoPasswordManagerWidgetFeature),
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
        VARIATION_ENTRY(kIPHAutofillFeedbackNewBadgeFeature),
        VARIATION_ENTRY(kIPHBatterySaverModeFeature),
        VARIATION_ENTRY(kIPHCompanionSidePanelFeature),
        VARIATION_ENTRY(kIPHCompanionSidePanelRegionSearchFeature),
        VARIATION_ENTRY(kIPHDesktopCustomizeChromeFeature),
        VARIATION_ENTRY(kIPHDesktopCustomizeChromeRefreshFeature),
        VARIATION_ENTRY(kIPHDesktopTabGroupsNewGroupFeature),
        VARIATION_ENTRY(kIPHDownloadToolbarButtonFeature),
        VARIATION_ENTRY(kIPHExtensionsMenuFeature),
        VARIATION_ENTRY(kIPHExtensionsRequestAccessButtonFeature),
        VARIATION_ENTRY(kIPHFocusModeFeature),
        VARIATION_ENTRY(kIPHGlobalMediaControls),
        VARIATION_ENTRY(kIPHGMCCastStartStopFeature),
        VARIATION_ENTRY(kIPHHighEfficiencyModeFeature),
        VARIATION_ENTRY(kIPHLiveCaption),
        VARIATION_ENTRY(kIPHPasswordsAccountStorageFeature),
        VARIATION_ENTRY(kIPHPasswordsManagementBubbleAfterSaveFeature),
        VARIATION_ENTRY(kIPHPasswordsManagementBubbleDuringSigninFeature),
        VARIATION_ENTRY(kIPHPasswordsWebAppProfileSwitchFeature),
        VARIATION_ENTRY(kIPHPasswordManagerShortcutFeature),
        VARIATION_ENTRY(kIPHPerformanceNewBadgeFeature),
        VARIATION_ENTRY(kIPHPowerBookmarksSidePanelFeature),
        VARIATION_ENTRY(kIPHPriceInsightsPageActionIconLabelFeature),
        VARIATION_ENTRY(kIPHPriceTrackingChipFeature),
        VARIATION_ENTRY(kIPHPriceTrackingEmailConsentFeature),
        VARIATION_ENTRY(kIPHPriceTrackingPageActionIconLabelFeature),
        VARIATION_ENTRY(kIPHReadingListDiscoveryFeature),
        VARIATION_ENTRY(kIPHReadingListEntryPointFeature),
        VARIATION_ENTRY(kIPHReadingListInSidePanelFeature),
        VARIATION_ENTRY(kIPHReadingModeSidePanelFeature),
        VARIATION_ENTRY(kIPHShoppingCollectionFeature),
        VARIATION_ENTRY(kIPHSideSearchAutoTriggeringFeature),
        VARIATION_ENTRY(kIPHSideSearchFeature),
        VARIATION_ENTRY(kIPHSideSearchPageActionLabelFeature),
        VARIATION_ENTRY(kIPHTabAudioMutingFeature),
        VARIATION_ENTRY(kIPHTabSearchFeature),
        VARIATION_ENTRY(kIPHWebUITabStripFeature),
        VARIATION_ENTRY(kIPHDesktopPwaInstallFeature),
        VARIATION_ENTRY(kIPHProfileSwitchFeature),
        VARIATION_ENTRY(kIPHDesktopSharedHighlightingFeature),
        VARIATION_ENTRY(kIPHWebUiHelpBubbleTestFeature),
        VARIATION_ENTRY(kIPHPriceTrackingInSidePanelFeature),
        VARIATION_ENTRY(kIPHBackNavigationMenuFeature),
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
        VARIATION_ENTRY(kIPHAutofillExternalAccountProfileSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillVirtualCardCVCSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillVirtualCardSuggestionFeature),
        VARIATION_ENTRY(kIPHCookieControlsFeature),
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS_ASH)
        VARIATION_ENTRY(kIPHGoogleOneOfferNotificationFeature),
        VARIATION_ENTRY(kIPHLauncherSearchHelpUiFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedOneFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedTwoFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedThreeFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedFourFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedFiveFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedSixFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedSevenFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedEightFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedNineFeature),
        VARIATION_ENTRY(kIPHScalableIphTimerBasedTenFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedOneFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedTwoFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedThreeFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedFourFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedFiveFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedSixFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedSevenFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedEightFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedNineFeature),
        VARIATION_ENTRY(kIPHScalableIphUnlockedBasedTenFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedNudgeFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedOneFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedTwoFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedThreeFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedFourFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedFiveFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedSixFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedSevenFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedEightFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedNineFeature),
        VARIATION_ENTRY(kIPHScalableIphHelpAppBasedTenFeature),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
        VARIATION_ENTRY(kIPHiOSPasswordPromoDesktopFeature),
#endif  // !BUILDFLAG(IS_ANDROID) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
};

#undef DEFINE_VARIATION_PARAM
#undef VARIATION_ENTRY

// Returns all the features that are in use for engagement tracking.
FeatureVector GetAllFeatures();

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
