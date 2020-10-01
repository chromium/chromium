/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

const base::Feature kIPHDemoMode{"IPH_DemoMode",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIPHDummyFeature{"IPH_Dummy",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
const base::Feature kIPHDesktopTabGroupsNewGroupFeature{
    "IPH_DesktopTabGroupsNewGroup", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHFocusModeFeature{"IPH_FocusMode",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHGlobalMediaControlsFeature{
    "IPH_GlobalMediaControls", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHLiveCaptionFeature{"IPH_LiveCaption",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPasswordsAccountStorageFeature{
    "IPH_PasswordsAccountStorage", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHReopenTabFeature{"IPH_ReopenTab",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHWebUITabStripFeature{"IPH_WebUITabStrip",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDesktopSnoozeFeature{"IPH_DesktopSnoozeFeature",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
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
const base::Feature kIPHDownloadSettingsFeature{
    "IPH_DownloadSettings", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadInfoBarDownloadContinuingFeature{
    "IPH_DownloadInfoBarDownloadContinuing", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadInfoBarDownloadsAreFasterFeature{
    "IPH_DownloadInfoBarDownloadsAreFaster", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHQuietNotificationPromptsFeature{
    "IPH_QuietNotificationPrompts", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHEphemeralTabFeature{"IPH_EphemeralTab",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHFeedCardMenuFeature{"IPH_FeedCardMenu",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHHomepagePromoCardFeature{
    "IPH_HomepagePromoCard", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHIdentityDiscFeature{"IPH_IdentityDisc",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryAddressFillingFeature{
    "IPH_KeyboardAccessoryAddressFilling", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryBarSwipingFeature{
    "IPH_KeyboardAccessoryBarSwiping", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryPasswordFillingFeature{
    "IPH_KeyboardAccessoryPasswordFilling", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHKeyboardAccessoryPaymentFillingFeature{
    "IPH_KeyboardAccessoryPaymentFilling", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPreviewsOmniboxUIFeature{
    "IPH_PreviewsOmniboxUI", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHTabGroupsQuicklyComparePagesFeature{
    "IPH_TabGroupsQuicklyComparePages", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTabGroupsTapToSeeAnotherTabFeature{
    "IPH_TabGroupsTapToSeeAnotherTab", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTabGroupsYourTabsAreTogetherFeature{
    "IPH_TabGroupsYourTabsTogether", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTabGroupsDragAndDropFeature{
    "IPH_TabGroupsDragAndDrop", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHTranslateMenuButtonFeature{
    "IPH_TranslateMenuButton", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHVideoTutorialDownloadFeature{
    "IPH_VideoTutorial_Download", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHVideoTutorialSearchFeature{
    "IPH_VideoTutorial_Search", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kIPHExploreSitesTileFeature{
    "IPH_ExploreSitesTile", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHFeedHeaderMenuFeature{
    "IPH_FeedHeaderMenu", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeReengagementNotification1Feature{
    "IPH_ChromeReengagementNotification1", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeReengagementNotification2Feature{
    "IPH_ChromeReengagementNotification2", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeReengagementNotification3Feature{
    "IPH_ChromeReengagementNotification3", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if defined(OS_IOS)
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
const base::Feature kIPHBadgedTranslateManualTriggerFeature{
    "IPH_BadgedTranslateManualTrigger", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDiscoverFeedHeaderFeature{
    "IPH_DiscoverFeedHeaderMenu", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_IOS)

}  // namespace feature_engagement
