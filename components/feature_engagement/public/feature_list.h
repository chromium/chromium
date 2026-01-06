// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_

#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/webui/flags/feature_entry.h"
#include "extensions/buildflags/buildflags.h"

namespace feature_engagement {
using FeatureVector = std::vector<const base::Feature*>;

// The param name for the FeatureVariation configuration, which is used by
// chrome://flags to set the variable name for the selected feature. The Tracker
// backend will then read this to figure out which feature (if any) was selected
// by the end user.
inline constexpr char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

// Defines a const flags_ui::FeatureEntry::FeatureParam for the given
// base::Feature. The constant name will be on the form
// kFooFeature --> kFooFeatureVariation. The |feature_name| argument must
// match the base::Feature::name member of the |base_feature|.
// This is intended to be used with VARIATION_ENTRY below to be able to insert
// it into an array of flags_ui::FeatureEntry::FeatureVariation.
#define DEFINE_VARIATION_PARAM(base_feature, feature_name)     \
  using validate_##base_feature##_t = decltype(&base_feature); \
  inline constexpr flags_ui::FeatureEntry::FeatureParam        \
      base_feature##Variation[] = {                            \
          {kIPHDemoModeFeatureChoiceParam, feature_name}}

// Defines a single flags_ui::FeatureEntry::FeatureVariation entry, fully
// enclosed. This is intended to be used with the declaration of
// |kIPHDemoModeChoiceVariations| below.
#define VARIATION_ENTRY(base_feature)                               \
  {base_feature##Variation[0].param_value, base_feature##Variation, \
   std::size(base_feature##Variation), nullptr}

// Defines a flags_ui::FeatureEntry::FeatureParam for each feature.
DEFINE_VARIATION_PARAM(kIPHDummyFeature, "IPH_Dummy");
#if BUILDFLAG(IS_ANDROID)
DEFINE_VARIATION_PARAM(kIPHAccountSettingsHistorySync,
                       "IPH_AccountSettingsHistorySync");
DEFINE_VARIATION_PARAM(kIPHAndroidTabDeclutter, "IPH_AndroidTabDeclutter");
DEFINE_VARIATION_PARAM(kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
                       "IPH_AdaptiveButtonInTopToolbarCustomization_NewTab");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationOpenInBrowserFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_OpenInBrowser");
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
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryWebFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_PageSummary_Web");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryPdfFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_PageSummary_Pdf");
DEFINE_VARIATION_PARAM(kIPHPageSummaryWebMenuFeature, "IPH_PageSummaryWebMenu");
DEFINE_VARIATION_PARAM(kIPHPageSummaryPdfMenuFeature, "IPH_PageSummaryPdfMenu");
DEFINE_VARIATION_PARAM(kIPHAutoDarkOptOutFeature, "IPH_AutoDarkOptOut");
DEFINE_VARIATION_PARAM(kIPHAutoDarkUserEducationMessageFeature,
                       "IPH_AutoDarkUserEducationMessage");
DEFINE_VARIATION_PARAM(kIPHAutoDarkUserEducationMessageOptInFeature,
                       "IPH_AutoDarkUserEducationMessageOptIn");
DEFINE_VARIATION_PARAM(kIPHAppSpecificHistory, "IPH_AppSpecificHistory");
DEFINE_VARIATION_PARAM(kIPHBookmarksBarFeature, "IPH_BookmarksBar");
DEFINE_VARIATION_PARAM(kIPHCCTHistory, "IPH_CCTHistory");
DEFINE_VARIATION_PARAM(kIPHCCTMinimized, "IPH_CCTMinimized");
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
DEFINE_VARIATION_PARAM(kIPHDefaultBrowserPromoMagicStackFeature,
                       "IPH_DefaultBrowserPromoMagicStack");
DEFINE_VARIATION_PARAM(kIPHDefaultBrowserPromoMessagesFeature,
                       "IPH_DefaultBrowserPromoMessages");
DEFINE_VARIATION_PARAM(kIPHDefaultBrowserPromoSettingCardFeature,
                       "IPH_DefaultBrowserPromoSettingCard");
DEFINE_VARIATION_PARAM(kIPHDownloadSettingsFeature, "IPH_DownloadSettings");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadContinuingFeature,
                       "IPH_DownloadInfoBarDownloadContinuing");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadsAreFasterFeature,
                       "IPH_DownloadInfoBarDownloadsAreFaster");
DEFINE_VARIATION_PARAM(kIPHEphemeralTabFeature, "IPH_EphemeralTab");
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
DEFINE_VARIATION_PARAM(kIPHMenuAddToGroup, "IPH_MenuAddToGroup");
DEFINE_VARIATION_PARAM(kIPHMostVisitedTilesCustomizationPinFeature,
                       "IPH_MostVisitedTilesCustomizationPin");
DEFINE_VARIATION_PARAM(kIPHPageInfoFeature, "IPH_PageInfo");
DEFINE_VARIATION_PARAM(kIPHPageInfoStoreInfoFeature, "IPH_PageInfoStoreInfo");
DEFINE_VARIATION_PARAM(kIPHPageZoomFeature, "IPH_PageZoom");
DEFINE_VARIATION_PARAM(kIPHPdfPageDownloadFeature, "IPH_PdfPageDownload");
DEFINE_VARIATION_PARAM(kIPHPreviewsOmniboxUIFeature, "IPH_PreviewsOmniboxUI");
DEFINE_VARIATION_PARAM(kIPHReadAloudAppMenuFeature,
                       "IPH_ReadAloudAppMenuFeature");
DEFINE_VARIATION_PARAM(kIPHReadAloudExpandedPlayerFeature,
                       "IPH_ReadAloudExpandedPlayerFeature");
DEFINE_VARIATION_PARAM(kIPHReadAloudPlaybackModeFeature,
                       "IPH_ReadAloudPlaybackModeFeature");
DEFINE_VARIATION_PARAM(kIPHReaderModeDistillInAppFeature,
                       "IPH_ReaderModeDistillInApp");
DEFINE_VARIATION_PARAM(kIPHReadLaterContextMenuFeature,
                       "IPH_ReadLaterContextMenu");
DEFINE_VARIATION_PARAM(kIPHReadLaterAppMenuBookmarkThisPageFeature,
                       "IPH_ReadLaterAppMenuBookmarkThisPage");
DEFINE_VARIATION_PARAM(kIPHReadLaterAppMenuBookmarksFeature,
                       "IPH_ReadLaterAppMenuBookmarks");
DEFINE_VARIATION_PARAM(kIPHReadLaterBottomSheetFeature,
                       "IPH_ReadLaterBottomSheet");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteDefaultOnFeature,
                       "IPH_RequestDesktopSiteDefaultOn");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteExceptionsGenericFeature,
                       "IPH_RequestDesktopSiteExceptionsGeneric");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteWindowSettingFeature,
                       "IPH_RequestDesktopSiteWindowSetting");
DEFINE_VARIATION_PARAM(kIPHShoppingListMenuItemFeature,
                       "IPH_ShoppingListMenuItem");
DEFINE_VARIATION_PARAM(kIPHShoppingListSaveFlowFeature,
                       "IPH_ShoppingListSaveFlow");
DEFINE_VARIATION_PARAM(kIPHTabGroupCreationDialogSyncTextFeature,
                       "IPH_TabGroupCreationDialogSyncText");
DEFINE_VARIATION_PARAM(kIPHTabGroupsDragAndDropFeature,
                       "IPH_TabGroupsDragAndDrop");
DEFINE_VARIATION_PARAM(kIPHTabGroupShareNoticeFeature,
                       "IPH_TabGroupShareNotice");
DEFINE_VARIATION_PARAM(kIPHTabGroupShareNotificationBubbleOnStripFeature,
                       "IPH_TabGroupSharedNotificationBubbleOnStrip");
DEFINE_VARIATION_PARAM(kIPHTabGroupShareUpdateFeature,
                       "IPH_TabGroupShareUpdate");
DEFINE_VARIATION_PARAM(kIPHTabGroupShareVersionUpdateFeature,
                       "IPH_TabGroupShareVersionUpdate");
DEFINE_VARIATION_PARAM(kIPHTabGroupsRemoteGroupFeature,
                       "IPH_TabGroupsRemoteGroup");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSurfaceFeature, "IPH_TabGroupsSurface");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSurfaceOnHideFeature,
                       "IPH_TabGroupsSurfaceOnHide");
DEFINE_VARIATION_PARAM(kIPHTabGroupSyncOnStripFeature,
                       "IPH_TabGroupSyncOnStrip");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherAddToGroup, "IPH_TabSwitcherAddToGroup");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherButtonFeature, "IPH_TabSwitcherButton");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherButtonSwitchIncognitoFeature,
                       "IPH_TabSwitcherButtonSwitchIncognito");
DEFINE_VARIATION_PARAM(kIPHTouchToSearchCalloutFeature,
                       "IPH_TouchToSearchCallout");
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
DEFINE_VARIATION_PARAM(kIPHWebFeedPostFollowDialogFeatureWithUIUpdate,
                       "IPH_WebFeedPostFollowDialogWithUIUpdate");
DEFINE_VARIATION_PARAM(kIPHSharedHighlightingBuilder,
                       "IPH_SharedHighlightingBuilder");
DEFINE_VARIATION_PARAM(kIPHSharedHighlightingReceiverFeature,
                       "IPH_SharedHighlightingReceiver");
DEFINE_VARIATION_PARAM(kIPHSharingHubWebnotesStylizeFeature,
                       "IPH_SharingHubWebnotesStylize");
DEFINE_VARIATION_PARAM(kIPHRestoreTabsOnFREFeature, "IPH_RestoreTabsOnFRE");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherXR, "IPH_TabSwitcherXR");
DEFINE_VARIATION_PARAM(kIPHTabTearingXR, "IPH_TabTearingXR");
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
DEFINE_VARIATION_PARAM(kIPHBottomToolbarTipFeature, "IPH_BottomToolbarTip");
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
DEFINE_VARIATION_PARAM(kIPHiOSLensOverlayEntrypointTipFeature,
                       "IPH_iOSLensOverlayEntrypointTip");
DEFINE_VARIATION_PARAM(kIPHiOSLensOverlayEscapeHatchTipFeature,
                       "IPH_iOSLensOverlayEscapeHatchTip");
DEFINE_VARIATION_PARAM(kIPHLongPressToolbarTipFeature,
                       "IPH_LongPressToolbarTip");
DEFINE_VARIATION_PARAM(kIPHBadgedReaderModeFeature, "IPH_BadgedReaderMode");
DEFINE_VARIATION_PARAM(kIPHBadgedReadingListFeature, "IPH_BadgedReadingList");
DEFINE_VARIATION_PARAM(kIPHWhatsNewUpdatedFeature, "IPH_WhatsNewUpdated");
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
                       "IPH_PriceNotificationsWhileBrowsing");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserOverflowMenuBadgeFeature,
                       "IPH_iOSDefaultBrowserOverflowMenuBadge");
DEFINE_VARIATION_PARAM(kIPHiOSFeedSwipeAnimatedFeature,
                       "IPH_iOSFeedSwipeAnimatedFeature");
DEFINE_VARIATION_PARAM(kIPHiOSFeedSwipeStaticFeature,
                       "IPH_iOSFeedSwipeStaticFeature");
DEFINE_VARIATION_PARAM(kIPHiOSPromoAppStoreFeature, "IPH_iOSPromoAppStore");
DEFINE_VARIATION_PARAM(kIPHiOSLensKeyboardFeature, "IPH_iOSLensKeyboard");
DEFINE_VARIATION_PARAM(kIPHiOSPromoWhatsNewFeature, "IPH_iOSPromoWhatsNew");
DEFINE_VARIATION_PARAM(kIPHiOSPromoSigninFullscreenFeature,
                       "IPH_iOSPromoSigninFullscreen");
DEFINE_VARIATION_PARAM(kIPHiOSPromoPostRestoreFeature,
                       "IPH_iOSPromoPostRestore");
DEFINE_VARIATION_PARAM(kIPHiOSPromoCredentialProviderExtensionFeature,
                       "IPH_iOSPromoCredentialProviderExtension");
DEFINE_VARIATION_PARAM(kIPHiOSPromoDefaultBrowserReminderFeature,
                       "IPH_iOSPromoDefaultBrowserReminder");
DEFINE_VARIATION_PARAM(kIPHiOSPromoNonModalSigninPasswordFeature,
                       "IPH_iOSPromoNonModalSigninPassword");
DEFINE_VARIATION_PARAM(kIPHiOSPromoNonModalSigninBookmarkFeature,
                       "IPH_iOSPromoNonModalSigninBookmark");
DEFINE_VARIATION_PARAM(kIPHiOSHistoryOnOverflowMenuFeature,
                       "IPH_iOSHistoryOnOverflowMenuFeature");
DEFINE_VARIATION_PARAM(kIPHiOSPromoPostRestoreDefaultBrowserFeature,
                       "IPH_iOSPromoPostRestoreDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature,
                       "IPH_iOSPromoNonModalUrlPasteDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature,
                       "IPH_iOSPromoNonModalAppSwitcherDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoNonModalShareDefaultBrowserFeature,
                       "IPH_iOSPromoNonModalShareDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoPasswordManagerWidgetFeature,
                       "IPH_iOSPromoPasswordManagerWidget");
DEFINE_VARIATION_PARAM(kIPHiOSPullToRefreshFeature,
                       "IPH_iOSPullToRefreshFeature");
DEFINE_VARIATION_PARAM(kIPHiOSReplaceSyncPromosWithSignInPromos,
                       "IPH_iOSReplaceSyncPromosWithSignInPromos");
DEFINE_VARIATION_PARAM(kIPHiOSTabGridSwipeRightForIncognito,
                       "IPH_iOSTabGridSwipeRightForIncognito");
DEFINE_VARIATION_PARAM(kIPHiOSDockingPromoFeature, "IPH_iOSDockingPromo");
DEFINE_VARIATION_PARAM(kIPHiOSDockingPromoRemindMeLaterFeature,
                       "IPH_iOSDockingPromoRemindMeLater");
DEFINE_VARIATION_PARAM(kIPHiOSPromoAllTabsFeature, "IPH_iOSPromoAllTabs");
DEFINE_VARIATION_PARAM(kIPHiOSPromoMadeForIOSFeature, "IPH_iOSPromoMadeForIOS");
DEFINE_VARIATION_PARAM(kIPHiOSPromoStaySafeFeature, "IPH_iOSPromoStaySafe");
DEFINE_VARIATION_PARAM(kIPHiOSSwipeBackForwardFeature,
                       "IPH_iOSSwipeBackForward");
DEFINE_VARIATION_PARAM(kIPHiOSSwipeToolbarToChangeTabFeature,
                       "IPH_iOSSwipeToolbarToChangeTab");
DEFINE_VARIATION_PARAM(kIPHiOSPostDefaultAbandonmentPromoFeature,
                       "IPH_iOSPostDefaultAbandonmentPromo");
DEFINE_VARIATION_PARAM(kIPHiOSPromoGenericDefaultBrowserFeature,
                       "IPH_iOSPromoGenericDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSOverflowMenuCustomizationFeature,
                       "IPH_iOSOverflowMenuCustomization");
DEFINE_VARIATION_PARAM(kIPHiOSPageInfoRevampFeature, "IPH_iOSPageInfoRevamp");
DEFINE_VARIATION_PARAM(kIPHiOSInlineEnhancedSafeBrowsingPromoFeature,
                       "IPH_iOSInlineEnhancedSafeBrowsingPromo");
DEFINE_VARIATION_PARAM(kIPHiOSSavedTabGroupClosed,
                       "IPH_iOSSavedTabGroupClosed");
DEFINE_VARIATION_PARAM(kIPHiOSContextualPanelSampleModelFeature,
                       "IPH_iOSContextualPanelSampleModel");
DEFINE_VARIATION_PARAM(kIPHiOSContextualPanelPriceInsightsFeature,
                       "IPH_iOSContextualPanelPriceInsights");
DEFINE_VARIATION_PARAM(kIPHHomeCustomizationMenuFeature,
                       "IPH_HomeCustomizationMenu");
DEFINE_VARIATION_PARAM(kIPHiOSSharedTabGroupForeground,
                       "IPH_iOSSharedTabGroupForeground");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserBannerPromoFeature,
                       "IPH_iOSDefaultBrowserBannerPromoFeature");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserOffCyclePromoFeature,
                       "IPH_iOSDefaultBrowserOffCyclePromo");
DEFINE_VARIATION_PARAM(kIPHiOSOneTimeDefaultBrowserNotificationFeature,
                       "IPH_iOSOneTimeDefaultBrowserNotification");
DEFINE_VARIATION_PARAM(kIPHiOSReminderNotificationsOverflowMenuBubbleFeature,
                       "IPH_iOSReminderNotificationsOverflowMenuBubbleFeature");
DEFINE_VARIATION_PARAM(
    kIPHiOSReminderNotificationsOverflowMenuNewBadgeFeature,
    "IPH_iOSReminderNotificationsOverflowMenuNewBadgeFeature");
DEFINE_VARIATION_PARAM(kIPHiOSDownloadAutoDeletionFeature,
                       "IPH_iOSDownloadAutoDeletion");
DEFINE_VARIATION_PARAM(kIPHiOSSettingsInOverflowMenuBubbleFeature,
                       "IPH_iOSSettingsInOverflowMenuBubbleFeature");
DEFINE_VARIATION_PARAM(
    kIPHiOSSwitchAccountsWithNTPAccountParticleDiscFeature,
    "IPH_iOSSwitchAccountsWithNTPAccountParticleDiscFeature");
DEFINE_VARIATION_PARAM(kIPHiOSWelcomeBackFeature, "IPH_iOSWelcomeBack");
DEFINE_VARIATION_PARAM(kIPHiOSSafariImportFeature,
                       "IPH_iOSSafariImportFeature");
DEFINE_VARIATION_PARAM(kIPHIOSPageActionMenu, "IPH_iOSPageActionMenu");
DEFINE_VARIATION_PARAM(kIPHiOSHomepageLensNewBadge,
                       "IPH_iOSHomepageLensNewBadge");
DEFINE_VARIATION_PARAM(kIPHiOSHomepageCustomizationNewBadge,
                       "IPH_iOSHomepageCustomizationNewBadge");
DEFINE_VARIATION_PARAM(kIPHiOSAIHubNewBadge, "IPH_iOSAIHubNewBadge");
DEFINE_VARIATION_PARAM(kIPHiOSGeminiContextualCueChip,
                       "IPH_iOSGeminiContextualCueChip");
DEFINE_VARIATION_PARAM(kIPHiOSGeminiFullscreenPromoFeature,
                       "IPH_iOSGeminiFullscreenPromoFeature");

#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
DEFINE_VARIATION_PARAM(kEsbDownloadRowPromoFeature, "EsbDownloadRowPromo");
#endif
DEFINE_VARIATION_PARAM(kIPHBatterySaverModeFeature, "IPH_BatterySaverMode");
DEFINE_VARIATION_PARAM(kIPHCompanionSidePanelFeature, "IPH_CompanionSidePanel");
DEFINE_VARIATION_PARAM(kIPHCompanionSidePanelRegionSearchFeature,
                       "IPH_CompanionSidePanelRegionSearch");
DEFINE_VARIATION_PARAM(kIPHComposeNewBadgeFeature,
                       "IPH_ComposeNewBadgeFeature");
DEFINE_VARIATION_PARAM(kIPHComposeMSBBSettingsFeature,
                       "IPH_ComposeMSBBSettingsFeature");
DEFINE_VARIATION_PARAM(kIPHDesktopCustomizeChromeAutoOpenFeature,
                       "IPH_DesktopCustomizeChromeAutoOpen");
DEFINE_VARIATION_PARAM(kIPHDesktopCustomizeChromeExperimentFeature,
                       "IPH_DesktopCustomizeChromeExperiment");
DEFINE_VARIATION_PARAM(kIPHDiscardRingFeature, "IPH_DiscardRing");
DEFINE_VARIATION_PARAM(kIPHDownloadEsbPromoFeature, "IPH_DownloadEsbPromo");
DEFINE_VARIATION_PARAM(kIPHExplicitBrowserSigninPreferenceRememberedFeature,
                       "IPH_ExplicitBrowserSigninPreferenceRemembered");
DEFINE_VARIATION_PARAM(kIPHGlicPromoFeature, "IPH_GlicPromo");
DEFINE_VARIATION_PARAM(kIPHGlicTrustFirstOnboardingShortcutSnoozePromoFeature,
                       "IPH_GlicTrustFirstOnboardingShortcutSnoozePromo");
DEFINE_VARIATION_PARAM(kIPHGlicTrustFirstOnboardingShortcutToastPromoFeature,
                       "IPH_GlicTrustFirstOnboardingShortcutToastPromo");
DEFINE_VARIATION_PARAM(kIPHGlicTryItFeature, "IPH_GlicTryIt");
DEFINE_VARIATION_PARAM(kIPHHistorySearchFeature, "IPH_HistorySearch");
#if BUILDFLAG(ENABLE_EXTENSIONS)
DEFINE_VARIATION_PARAM(kIPHExtensionsMenuFeature, "IPH_ExtensionsMenu");
DEFINE_VARIATION_PARAM(kIPHExtensionsRequestAccessButtonFeature,
                       "IPH_ExtensionsRequestAccessButton");
DEFINE_VARIATION_PARAM(kIPHExtensionsZeroStatePromoFeature,
                       "IPH_ExtensionsZeroStatePromo");
#endif
DEFINE_VARIATION_PARAM(kIPHGMCCastStartStopFeature, "IPH_GMCCastStartStop");
DEFINE_VARIATION_PARAM(kIPHGMCLocalMediaCastingFeature,
                       "IPH_GMCLocalMediaCasting");
// The feature is used in Finch experiments so it is unable to be renamed
// alongside the variable name.
DEFINE_VARIATION_PARAM(kIPHMemorySaverModeFeature, "IPH_HighEfficiencyMode");
DEFINE_VARIATION_PARAM(kIPHLensOverlayFeature, "IPH_LensOverlay");
DEFINE_VARIATION_PARAM(kIPHLensOverlayTranslateButtonFeature,
                       "IPH_LensOverlayTranslateButton");
DEFINE_VARIATION_PARAM(kIPHLiveCaptionFeature, "IPH_LiveCaption");
DEFINE_VARIATION_PARAM(kIPHMerchantTrustFeature, "IPH_MerchantTrust");
DEFINE_VARIATION_PARAM(kIPHPasswordsSavePrimingPromoFeature,
                       "IPH_PasswordsSavePrimingPromo");
DEFINE_VARIATION_PARAM(kIPHPasswordsSaveRecoveryPromoFeature,
                       "IPH_PasswordsSaveRecoveryPromo");
DEFINE_VARIATION_PARAM(kIPHPasswordsManagementBubbleAfterSaveFeature,
                       "IPH_PasswordsManagementBubbleAfterSave");
DEFINE_VARIATION_PARAM(kIPHPasswordsManagementBubbleDuringSigninFeature,
                       "IPH_PasswordsManagementBubbleDuringSignin");
DEFINE_VARIATION_PARAM(kIPHPasswordsWebAppProfileSwitchFeature,
                       "IPH_PasswordsWebAppProfileSwitch");
DEFINE_VARIATION_PARAM(kIPHPasswordManagerShortcutFeature,
                       "IPH_PasswordManagerShortcut");
DEFINE_VARIATION_PARAM(kIPHPasswordSharingFeature,
                       "IPH_PasswordSharingFeature");
DEFINE_VARIATION_PARAM(kIPHPdfInkSignaturesFeature, "IPH_PdfInkSignatures");
DEFINE_VARIATION_PARAM(kIPHPdfSearchifyFeature, "IPH_PdfSearchifyFeature");
DEFINE_VARIATION_PARAM(kIPHPerformanceInterventionDialogFeature,
                       "IPH_PerformanceInterventionDialogFeature");
DEFINE_VARIATION_PARAM(kIPHPlusAddressFirstSaveFeature,
                       "IPH_PlusAddressFirstSaveFeature");
DEFINE_VARIATION_PARAM(kIPHPowerBookmarksSidePanelFeature,
                       "IPH_PowerBookmarksSidePanel");
DEFINE_VARIATION_PARAM(kIPHPriceInsightsPageActionIconLabelFeature,
                       "IPH_PriceInsightsPageActionIconLabelFeature");
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
DEFINE_VARIATION_PARAM(kIPHReadingModePageActionLabelFeature,
                       "IPH_ReadingModePageActionLabel");
DEFINE_VARIATION_PARAM(kIPHShoppingCollectionFeature,
                       "IPH_ShoppingCollectionFeature");
DEFINE_VARIATION_PARAM(kIPHSideBySidePinnableFeature,
                       "IPH_SideBySidePinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSideBySideTabSwitchFeature,
                       "IPH_SideBySideTabSwitchFeature");
DEFINE_VARIATION_PARAM(kIPHSidePanelGenericPinnableFeature,
                       "IPH_SidePanelGenericPinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSidePanelLensOverlayPinnableFeature,
                       "IPH_SidePanelLensOverlayPinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSideSearchAutoTriggeringFeature,
                       "IPH_SideSearchAutoTriggering");
DEFINE_VARIATION_PARAM(kIPHSideSearchPageActionLabelFeature,
                       "IPH_SideSearchPageActionLabel");
DEFINE_VARIATION_PARAM(kIPHPwaQuietNotificationFeature,
                       "IPH_PwaQuietNotification");
DEFINE_VARIATION_PARAM(kIPHTabAudioMutingFeature, "IPH_TabAudioMuting");
DEFINE_VARIATION_PARAM(kIPHTabOrganizationSuccessFeature,
                       "IPH_TabOrganizationSuccess");
DEFINE_VARIATION_PARAM(kIPHTabSearchFeature, "IPH_TabSearch");
DEFINE_VARIATION_PARAM(kIPHTabSearchToolbarButtonFeature,
                       "IPH_TabSearchToolbarButton");
DEFINE_VARIATION_PARAM(kIPHDesktopPwaInstallFeature, "IPH_DesktopPwaInstall");
DEFINE_VARIATION_PARAM(kIPHProfileSwitchFeature, "IPH_ProfileSwitch");
DEFINE_VARIATION_PARAM(kIPHDesktopSharedHighlightingFeature,
                       "IPH_DesktopSharedHighlighting");
DEFINE_VARIATION_PARAM(kIPHWebUiHelpBubbleTestFeature,
                       "IPH_WebUiHelpBubbleTest");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingInSidePanelFeature,
                       "IPH_PriceTrackingInSidePanel");
DEFINE_VARIATION_PARAM(kIPHBackNavigationMenuFeature, "IPH_BackNavigationMenu");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSaveV2IntroFeature,
                       "IPH_TabGroupsSaveV2Intro");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSaveV2CloseGroupFeature,
                       "IPH_TabGroupsSaveV2CloseGroup");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSharedTabChangedFeature,
                       "IPH_TabGroupsSharedTabChanged");
DEFINE_VARIATION_PARAM(kIPHTabGroupsSharedTabFeedbackFeature,
                       "IPH_TabGroupsSharedTabFeedback");
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
DEFINE_VARIATION_PARAM(kIPHAutofillAiOptInFeature, "IPH_AutofillAiOptIn");
DEFINE_VARIATION_PARAM(kIPHAutofillAiValuablesFeature,
                       "IPH_AutofillAiValuables");
DEFINE_VARIATION_PARAM(kIPHAutofillBnplAffirmOrZipSuggestionFeature,
                       "IPH_AutofillBnplAffirmOrZipSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature,
                       "IPH_AutofillBnplAffirmZipOrKlarnaSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillCreditCardBenefitFeature,
                       "IPH_AutofillCreditCardBenefit");
DEFINE_VARIATION_PARAM(kIPHAutofillCardInfoRetrievalSuggestionFeature,
                       "IPH_AutofillCardInfoRetrievalSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillDisabledVirtualCardSuggestionFeature,
                       "IPH_AutofillDisabledVirtualCardSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillEnableLoyaltyCardsFeature,
                       "IPH_AutofillEnableLoyaltyCards");
DEFINE_VARIATION_PARAM(kIPHAutofillExternalAccountProfileSuggestionFeature,
                       "IPH_AutofillExternalAccountProfileSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillHomeWorkProfileSuggestionFeature,
                       "IPH_AutofillHomeWorkProfileSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillAccountNameEmailSuggestionFeature,
                       "IPH_AutofillAccountNameEmailSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillVirtualCardCVCSuggestionFeature,
                       "IPH_AutofillVirtualCardCVCSuggestion");
DEFINE_VARIATION_PARAM(kIPHAutofillVirtualCardSuggestionFeature,
                       "IPH_AutofillVirtualCardSuggestion");
DEFINE_VARIATION_PARAM(kIPHCookieControlsFeature, "IPH_CookieControls");
DEFINE_VARIATION_PARAM(kIPHPlusAddressCreateSuggestionFeature,
                       "IPH_PlusAddressCreateSuggestion");
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
DEFINE_VARIATION_PARAM(kIPHGrowthFramework, "IPH_GrowthFramework");
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
DEFINE_VARIATION_PARAM(kIPHScalableIphGamingFeature, "IPH_ScalableIphGaming");
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
DEFINE_VARIATION_PARAM(kIPHDesktopPWAsLinkCapturingLaunch,
                       "IPH_DesktopPWAsLinkCapturingLaunch");
DEFINE_VARIATION_PARAM(kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
                       "IPH_DesktopPWAsLinkCapturingLaunchAppInTab");
DEFINE_VARIATION_PARAM(kIPHSignInBenefitsFeature, "IPH_SignInBenefits");
DEFINE_VARIATION_PARAM(kIPHSupervisedUserProfileSigninFeature,
                       "IPH_SupervisedUserProfileSignin");
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_ANDROID)
DEFINE_VARIATION_PARAM(kIPHiOSPasswordPromoDesktopFeature,
                       "IPH_iOSPasswordPromoDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSAddressPromoDesktopFeature,
                       "IPH_iOSAddressPromoDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSPaymentPromoDesktopFeature,
                       "IPH_iOSPaymentPromoDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSLensPromoDesktopFeature,
                       "IPH_iOSLensPromoDesktop");
DEFINE_VARIATION_PARAM(kIPHiOSEnhancedBrowsingDesktopFeature,
                       "IPH_iOSEnhancedBrowsingDesktop");
#endif  // !BUILDFLAG(IS_ANDROID)

// Defines the array of which features should be listed in the chrome://flags
// UI to be able to select them alone for demo-mode. The features listed here
// are possible to enable on their own in demo mode.
inline constexpr flags_ui::FeatureEntry::FeatureVariation
    kIPHDemoModeChoiceVariations[] = {
#if BUILDFLAG(IS_ANDROID)
        VARIATION_ENTRY(kIPHAccountSettingsHistorySync),
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
        VARIATION_ENTRY(
            kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryWebFeature),
        VARIATION_ENTRY(
            kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryPdfFeature),
        VARIATION_ENTRY(kIPHPageSummaryWebMenuFeature),
        VARIATION_ENTRY(kIPHPageSummaryPdfMenuFeature),
        VARIATION_ENTRY(kIPHAndroidTabDeclutter),
        VARIATION_ENTRY(kIPHAutoDarkOptOutFeature),
        VARIATION_ENTRY(kIPHAutoDarkUserEducationMessageFeature),
        VARIATION_ENTRY(kIPHAutoDarkUserEducationMessageOptInFeature),
        VARIATION_ENTRY(kIPHAppSpecificHistory),
        VARIATION_ENTRY(kIPHBookmarksBarFeature),
        VARIATION_ENTRY(kIPHCCTHistory),
        VARIATION_ENTRY(kIPHCCTMinimized),
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
        VARIATION_ENTRY(kIPHDefaultBrowserPromoMagicStackFeature),
        VARIATION_ENTRY(kIPHDefaultBrowserPromoMessagesFeature),
        VARIATION_ENTRY(kIPHDefaultBrowserPromoSettingCardFeature),
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
        VARIATION_ENTRY(kIPHMenuAddToGroup),
        VARIATION_ENTRY(kIPHMostVisitedTilesCustomizationPinFeature),
        VARIATION_ENTRY(kIPHPageInfoFeature),
        VARIATION_ENTRY(kIPHPageInfoStoreInfoFeature),
        VARIATION_ENTRY(kIPHPageZoomFeature),
        VARIATION_ENTRY(kIPHPdfPageDownloadFeature),
        VARIATION_ENTRY(kIPHPreviewsOmniboxUIFeature),
        VARIATION_ENTRY(kIPHReaderModeDistillInAppFeature),
        VARIATION_ENTRY(kIPHReadAloudAppMenuFeature),
        VARIATION_ENTRY(kIPHReadAloudExpandedPlayerFeature),
        VARIATION_ENTRY(kIPHReadLaterContextMenuFeature),
        VARIATION_ENTRY(kIPHReadLaterAppMenuBookmarkThisPageFeature),
        VARIATION_ENTRY(kIPHReadLaterAppMenuBookmarksFeature),
        VARIATION_ENTRY(kIPHReadLaterBottomSheetFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteDefaultOnFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteExceptionsGenericFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteWindowSettingFeature),
        VARIATION_ENTRY(kIPHShoppingListMenuItemFeature),
        VARIATION_ENTRY(kIPHShoppingListSaveFlowFeature),
        VARIATION_ENTRY(kIPHTabGroupCreationDialogSyncTextFeature),
        VARIATION_ENTRY(kIPHTabGroupShareNoticeFeature),
        VARIATION_ENTRY(kIPHTabGroupShareUpdateFeature),
        VARIATION_ENTRY(kIPHTabGroupShareVersionUpdateFeature),
        VARIATION_ENTRY(kIPHTabGroupsDragAndDropFeature),
        VARIATION_ENTRY(kIPHTabGroupsRemoteGroupFeature),
        VARIATION_ENTRY(kIPHTabGroupsSurfaceFeature),
        VARIATION_ENTRY(kIPHTabGroupsSurfaceOnHideFeature),
        VARIATION_ENTRY(kIPHTabSwitcherAddToGroup),
        VARIATION_ENTRY(kIPHTabSwitcherButtonFeature),
        VARIATION_ENTRY(kIPHTabSwitcherButtonSwitchIncognitoFeature),
        VARIATION_ENTRY(kIPHTouchToSearchCalloutFeature),
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
        VARIATION_ENTRY(kIPHWebFeedPostFollowDialogFeatureWithUIUpdate),
        VARIATION_ENTRY(kIPHSharedHighlightingBuilder),
        VARIATION_ENTRY(kIPHSharedHighlightingReceiverFeature),
        VARIATION_ENTRY(kIPHSharingHubWebnotesStylizeFeature),
        VARIATION_ENTRY(kIPHRestoreTabsOnFREFeature),
        VARIATION_ENTRY(kIPHTabSwitcherXR),
        VARIATION_ENTRY(kIPHTabTearingXR),
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
        VARIATION_ENTRY(kIPHBottomToolbarTipFeature),
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
        VARIATION_ENTRY(kIPHLongPressToolbarTipFeature),
        VARIATION_ENTRY(kIPHBadgedReadingListFeature),
        VARIATION_ENTRY(kIPHReadingListMessagesFeature),
        VARIATION_ENTRY(kIPHWhatsNewFeature),
        VARIATION_ENTRY(kIPHWhatsNewUpdatedFeature),
        VARIATION_ENTRY(kIPHBadgedTranslateManualTriggerFeature),
        VARIATION_ENTRY(kIPHDiscoverFeedHeaderFeature),
        VARIATION_ENTRY(kIPHDefaultSiteViewFeature),
        VARIATION_ENTRY(kIPHFollowWhileBrowsingFeature),
        VARIATION_ENTRY(kIPHPriceNotificationsWhileBrowsingFeature),
        VARIATION_ENTRY(kIPHiOSDefaultBrowserOverflowMenuBadgeFeature),
        VARIATION_ENTRY(kIPHiOSLensKeyboardFeature),
        VARIATION_ENTRY(kIPHiOSPromoAppStoreFeature),
        VARIATION_ENTRY(kIPHiOSPromoWhatsNewFeature),
        VARIATION_ENTRY(kIPHiOSPromoPostRestoreFeature),
        VARIATION_ENTRY(kIPHiOSPromoCredentialProviderExtensionFeature),
        VARIATION_ENTRY(kIPHiOSHistoryOnOverflowMenuFeature),
        VARIATION_ENTRY(kIPHiOSPromoGenericDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoPostRestoreDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoNonModalShareDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoNonModalSigninPasswordFeature),
        VARIATION_ENTRY(kIPHiOSPromoNonModalSigninBookmarkFeature),
        VARIATION_ENTRY(kIPHiOSPromoPasswordManagerWidgetFeature),
        VARIATION_ENTRY(kIPHiOSPullToRefreshFeature),
        VARIATION_ENTRY(kIPHiOSReplaceSyncPromosWithSignInPromos),
        VARIATION_ENTRY(kIPHiOSTabGridSwipeRightForIncognito),
        VARIATION_ENTRY(kIPHiOSDockingPromoFeature),
        VARIATION_ENTRY(kIPHiOSDockingPromoRemindMeLaterFeature),
        VARIATION_ENTRY(kIPHiOSPromoAllTabsFeature),
        VARIATION_ENTRY(kIPHiOSPromoMadeForIOSFeature),
        VARIATION_ENTRY(kIPHiOSPromoStaySafeFeature),
        VARIATION_ENTRY(kIPHiOSSwipeBackForwardFeature),
        VARIATION_ENTRY(kIPHiOSSwipeToolbarToChangeTabFeature),
        VARIATION_ENTRY(kIPHiOSPostDefaultAbandonmentPromoFeature),
        VARIATION_ENTRY(kIPHiOSPromoGenericDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSOverflowMenuCustomizationFeature),
        VARIATION_ENTRY(kIPHiOSSavedTabGroupClosed),
        VARIATION_ENTRY(kIPHiOSContextualPanelSampleModelFeature),
        VARIATION_ENTRY(kIPHiOSContextualPanelPriceInsightsFeature),
        VARIATION_ENTRY(kIPHHomeCustomizationMenuFeature),
        VARIATION_ENTRY(kIPHiOSLensOverlayEntrypointTipFeature),
        VARIATION_ENTRY(kIPHiOSLensOverlayEscapeHatchTipFeature),
        VARIATION_ENTRY(kIPHiOSSharedTabGroupForeground),
        VARIATION_ENTRY(kIPHiOSDefaultBrowserBannerPromoFeature),
        VARIATION_ENTRY(kIPHiOSReminderNotificationsOverflowMenuBubbleFeature),
        VARIATION_ENTRY(
            kIPHiOSReminderNotificationsOverflowMenuNewBadgeFeature),
        VARIATION_ENTRY(kIPHiOSDownloadAutoDeletionFeature),
        VARIATION_ENTRY(kIPHiOSWelcomeBackFeature),
        VARIATION_ENTRY(kIPHiOSSafariImportFeature),
        VARIATION_ENTRY(kIPHIOSPageActionMenu),
        VARIATION_ENTRY(kIPHiOSAIHubNewBadge),
        VARIATION_ENTRY(kIPHiOSGeminiContextualCueChip),
        VARIATION_ENTRY(kIPHiOSGeminiFullscreenPromoFeature),
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
        VARIATION_ENTRY(kIPHBatterySaverModeFeature),
        VARIATION_ENTRY(kIPHCompanionSidePanelFeature),
        VARIATION_ENTRY(kIPHCompanionSidePanelRegionSearchFeature),
        VARIATION_ENTRY(kIPHComposeMSBBSettingsFeature),
        VARIATION_ENTRY(kIPHComposeNewBadgeFeature),
        VARIATION_ENTRY(kIPHDesktopCustomizeChromeExperimentFeature),
        VARIATION_ENTRY(kIPHDesktopCustomizeChromeAutoOpenFeature),
        VARIATION_ENTRY(kIPHDiscardRingFeature),
        VARIATION_ENTRY(kIPHExplicitBrowserSigninPreferenceRememberedFeature),
        VARIATION_ENTRY(kIPHGlicPromoFeature),
        VARIATION_ENTRY(kIPHGlicTrustFirstOnboardingShortcutSnoozePromoFeature),
        VARIATION_ENTRY(kIPHGlicTrustFirstOnboardingShortcutToastPromoFeature),
        VARIATION_ENTRY(kIPHGlicTryItFeature),
        VARIATION_ENTRY(kIPHPwaQuietNotificationFeature),
        VARIATION_ENTRY(kIPHHistorySearchFeature),
#if BUILDFLAG(ENABLE_EXTENSIONS)
        VARIATION_ENTRY(kIPHExtensionsMenuFeature),
        VARIATION_ENTRY(kIPHExtensionsRequestAccessButtonFeature),
#endif
        VARIATION_ENTRY(kIPHGMCCastStartStopFeature),
        VARIATION_ENTRY(kIPHGMCLocalMediaCastingFeature),
        VARIATION_ENTRY(kIPHMemorySaverModeFeature),
        VARIATION_ENTRY(kIPHLiveCaptionFeature),
        VARIATION_ENTRY(kIPHMerchantTrustFeature),
        VARIATION_ENTRY(kIPHPasswordsManagementBubbleAfterSaveFeature),
        VARIATION_ENTRY(kIPHPasswordsManagementBubbleDuringSigninFeature),
        VARIATION_ENTRY(kIPHPasswordsWebAppProfileSwitchFeature),
        VARIATION_ENTRY(kIPHPasswordManagerShortcutFeature),
        VARIATION_ENTRY(kIPHPasswordSharingFeature),
        VARIATION_ENTRY(kIPHPdfInkSignaturesFeature),
        VARIATION_ENTRY(kIPHPdfSearchifyFeature),
        VARIATION_ENTRY(kIPHPerformanceInterventionDialogFeature),
        VARIATION_ENTRY(kIPHPlusAddressFirstSaveFeature),
        VARIATION_ENTRY(kIPHPowerBookmarksSidePanelFeature),
        VARIATION_ENTRY(kIPHPriceInsightsPageActionIconLabelFeature),
        VARIATION_ENTRY(kIPHPriceTrackingEmailConsentFeature),
        VARIATION_ENTRY(kIPHPriceTrackingPageActionIconLabelFeature),
        VARIATION_ENTRY(kIPHReadingListDiscoveryFeature),
        VARIATION_ENTRY(kIPHReadingListEntryPointFeature),
        VARIATION_ENTRY(kIPHReadingListInSidePanelFeature),
        VARIATION_ENTRY(kIPHReadingModeSidePanelFeature),
        VARIATION_ENTRY(kIPHReadingModePageActionLabelFeature),
        VARIATION_ENTRY(kIPHShoppingCollectionFeature),
        VARIATION_ENTRY(kIPHSideBySidePinnableFeature),
        VARIATION_ENTRY(kIPHSideBySideTabSwitchFeature),
        VARIATION_ENTRY(kIPHSidePanelGenericPinnableFeature),
        VARIATION_ENTRY(kIPHSideSearchAutoTriggeringFeature),
        VARIATION_ENTRY(kIPHSideSearchPageActionLabelFeature),
        VARIATION_ENTRY(kIPHTabAudioMutingFeature),
        VARIATION_ENTRY(kIPHTabSearchFeature),
        VARIATION_ENTRY(kIPHTabSearchToolbarButtonFeature),
        VARIATION_ENTRY(kIPHTabGroupsSharedTabChangedFeature),
        VARIATION_ENTRY(kIPHTabGroupsSharedTabFeedbackFeature),
        VARIATION_ENTRY(kIPHTabOrganizationSuccessFeature),
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
        VARIATION_ENTRY(kIPHAutofillAiOptInFeature),
        VARIATION_ENTRY(kIPHAutofillAiValuablesFeature),
        VARIATION_ENTRY(kIPHAutofillCreditCardBenefitFeature),
        VARIATION_ENTRY(kIPHAutofillCardInfoRetrievalSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillDisabledVirtualCardSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillEnableLoyaltyCardsFeature),
        VARIATION_ENTRY(kIPHAutofillExternalAccountProfileSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillHomeWorkProfileSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillAccountNameEmailSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillVirtualCardCVCSuggestionFeature),
        VARIATION_ENTRY(kIPHAutofillVirtualCardSuggestionFeature),
        VARIATION_ENTRY(kIPHPlusAddressCreateSuggestionFeature),
        VARIATION_ENTRY(kIPHCookieControlsFeature),
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
        VARIATION_ENTRY(kIPHGrowthFramework),
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
        VARIATION_ENTRY(kIPHScalableIphGamingFeature),
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        VARIATION_ENTRY(kIPHDesktopPWAsLinkCapturingLaunch),
        VARIATION_ENTRY(kIPHDesktopPWAsLinkCapturingLaunchAppInTab),
        VARIATION_ENTRY(kIPHSignInBenefitsFeature),
        VARIATION_ENTRY(kIPHSupervisedUserProfileSigninFeature),
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_ANDROID)
        VARIATION_ENTRY(kIPHiOSPasswordPromoDesktopFeature),
        VARIATION_ENTRY(kIPHiOSAddressPromoDesktopFeature),
        VARIATION_ENTRY(kIPHiOSPaymentPromoDesktopFeature),
        VARIATION_ENTRY(kIPHiOSLensPromoDesktopFeature),
        VARIATION_ENTRY(kIPHiOSEnhancedBrowsingDesktopFeature),
#endif  // !BUILDFLAG(IS_ANDROID)
};

#undef DEFINE_VARIATION_PARAM
#undef VARIATION_ENTRY

// Returns all the features that are in use for engagement tracking.
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
FeatureVector GetAllFeatures();

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
