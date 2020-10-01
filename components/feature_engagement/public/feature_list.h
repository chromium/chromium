// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_

#include <vector>

#include "base/feature_list.h"
#include "base/stl_util.h"
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
        base::size(base_feature##Variation), nullptr                 \
  }

// Defines a flags_ui::FeatureEntry::FeatureParam for each feature.
DEFINE_VARIATION_PARAM(kIPHDummyFeature, "IPH_Dummy");
#if defined(OS_ANDROID)
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
DEFINE_VARIATION_PARAM(kIPHContextualSearchTranslationEnableFeature,
                       "IPH_ContextualSearchTranslationEnable");
DEFINE_VARIATION_PARAM(kIPHContextualSearchWebSearchFeature,
                       "IPH_ContextualSearchWebSearch");
DEFINE_VARIATION_PARAM(kIPHContextualSearchPromoteTapFeature,
                       "IPH_ContextualSearchPromoteTap");
DEFINE_VARIATION_PARAM(kIPHContextualSearchPromotePanelOpenFeature,
                       "IPH_ContextualSearchPromotePanelOpen");
DEFINE_VARIATION_PARAM(kIPHContextualSearchOptInFeature,
                       "IPH_ContextualSearchOptIn");
DEFINE_VARIATION_PARAM(kIPHContextualSearchTappedButShouldLongpressFeature,
                       "IPH_ContextualSearchTappedButShouldLongpress");
DEFINE_VARIATION_PARAM(kIPHDownloadSettingsFeature, "IPH_DownloadSettings");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadContinuingFeature,
                       "IPH_DownloadInfoBarDownloadContinuing");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadsAreFasterFeature,
                       "IPH_DownloadInfoBarDownloadsAreFaster");
DEFINE_VARIATION_PARAM(kIPHEphemeralTabFeature, "IPH_EphemeralTab");
DEFINE_VARIATION_PARAM(kIPHFeedCardMenuFeature, "IPH_FeedCardMenu");
DEFINE_VARIATION_PARAM(kIPHHomepagePromoCardFeature, "IPH_HomepagePromoCard");
DEFINE_VARIATION_PARAM(kIPHIdentityDiscFeature, "IPH_IdentityDisc");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryAddressFillingFeature,
                       "IPH_KeyboardAccessoryAddressFilling");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryBarSwipingFeature,
                       "IPH_KeyboardAccessoryBarSwiping");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryPasswordFillingFeature,
                       "IPH_KeyboardAccessoryPasswordFilling");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryPaymentFillingFeature,
                       "IPH_KeyboardAccessoryPaymentFilling");
DEFINE_VARIATION_PARAM(kIPHPreviewsOmniboxUIFeature, "IPH_PreviewsOmniboxUI");
DEFINE_VARIATION_PARAM(kIPHQuietNotificationPromptsFeature,
                       "IPH_QuietNotificationPrompts");
DEFINE_VARIATION_PARAM(kIPHTabGroupsQuicklyComparePagesFeature,
                       "IPH_TabGroupsQuicklyComparePages");
DEFINE_VARIATION_PARAM(kIPHTabGroupsTapToSeeAnotherTabFeature,
                       "IPH_TabGroupsTapToSeeAnotherTab");
DEFINE_VARIATION_PARAM(kIPHTabGroupsYourTabsAreTogetherFeature,
                       "IPH_TabGroupsYourTabsTogether");
DEFINE_VARIATION_PARAM(kIPHTabGroupsDragAndDropFeature,
                       "IPH_TabGroupsDragAndDrop");
DEFINE_VARIATION_PARAM(kIPHTranslateMenuButtonFeature,
                       "IPH_TranslateMenuButton");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialDownloadFeature,
                       "IPH_VideoTutorial_Download");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialSearchFeature,
                       "IPH_VideoTutorial_Search");
DEFINE_VARIATION_PARAM(kIPHExploreSitesTileFeature, "IPH_ExploreSitesTile");
DEFINE_VARIATION_PARAM(kIPHFeedHeaderMenuFeature, "IPH_FeedHeaderMenu");
#endif  // defined(OS_ANDROID)
#if defined(OS_IOS)
DEFINE_VARIATION_PARAM(kIPHBottomToolbarTipFeature, "IPH_BottomToolbarTip");
DEFINE_VARIATION_PARAM(kIPHLongPressToolbarTipFeature,
                       "IPH_LongPressToolbarTip");
DEFINE_VARIATION_PARAM(kIPHNewTabTipFeature, "IPH_NewTabTip");
DEFINE_VARIATION_PARAM(kIPHNewIncognitoTabTipFeature, "IPH_NewIncognitoTabTip");
DEFINE_VARIATION_PARAM(kIPHBadgedReadingListFeature, "IPH_BadgedReadingList");
DEFINE_VARIATION_PARAM(kIPHBadgedTranslateManualTriggerFeature,
                       "IPH_BadgedTranslateManualTrigger");
DEFINE_VARIATION_PARAM(kIPHDiscoverFeedHeaderFeature,
                       "IPH_DiscoverFeedHeaderMenu");
#endif  // defined(OS_IOS)

#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
DEFINE_VARIATION_PARAM(kIPHDesktopTabGroupsNewGroupFeature,
                       "IPH_DesktopTabGroupsNewGroup");
DEFINE_VARIATION_PARAM(kIPHFocusModeFeature, "IPH_FocusMode");
DEFINE_VARIATION_PARAM(kIPHGlobalMediaControls, "IPH_GlobalMediaControls");
DEFINE_VARIATION_PARAM(kIPHLiveCaption, "IPH_LiveCaption");
DEFINE_VARIATION_PARAM(kIPHPasswordsAccountStorageFeature,
                       "IPH_PasswordsAccountStorage");
DEFINE_VARIATION_PARAM(kIPHReopenTabFeature, "IPH_ReopenTab");
DEFINE_VARIATION_PARAM(kIPHWebUITabStripFeature, "IPH_WebUITabStrip");
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

}  // namespace

// Defines the array of which features should be listed in the chrome://flags
// UI to be able to select them alone for demo-mode. The features listed here
// are possible to enable on their own in demo mode.
constexpr flags_ui::FeatureEntry::FeatureVariation
    kIPHDemoModeChoiceVariations[] = {
#if defined(OS_ANDROID)
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
        VARIATION_ENTRY(kIPHContextualSearchTranslationEnableFeature),
        VARIATION_ENTRY(kIPHContextualSearchWebSearchFeature),
        VARIATION_ENTRY(kIPHContextualSearchPromoteTapFeature),
        VARIATION_ENTRY(kIPHContextualSearchPromotePanelOpenFeature),
        VARIATION_ENTRY(kIPHContextualSearchOptInFeature),
        VARIATION_ENTRY(kIPHContextualSearchTappedButShouldLongpressFeature),
        VARIATION_ENTRY(kIPHDownloadSettingsFeature),
        VARIATION_ENTRY(kIPHDownloadInfoBarDownloadContinuingFeature),
        VARIATION_ENTRY(kIPHDownloadInfoBarDownloadsAreFasterFeature),
        VARIATION_ENTRY(kIPHEphemeralTabFeature),
        VARIATION_ENTRY(kIPHFeedCardMenuFeature),
        VARIATION_ENTRY(kIPHHomepagePromoCardFeature),
        VARIATION_ENTRY(kIPHIdentityDiscFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryAddressFillingFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryBarSwipingFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryPasswordFillingFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryPaymentFillingFeature),
        VARIATION_ENTRY(kIPHPreviewsOmniboxUIFeature),
        VARIATION_ENTRY(kIPHQuietNotificationPromptsFeature),
        VARIATION_ENTRY(kIPHTabGroupsQuicklyComparePagesFeature),
        VARIATION_ENTRY(kIPHTabGroupsTapToSeeAnotherTabFeature),
        VARIATION_ENTRY(kIPHTabGroupsYourTabsAreTogetherFeature),
        VARIATION_ENTRY(kIPHTabGroupsDragAndDropFeature),
        VARIATION_ENTRY(kIPHTranslateMenuButtonFeature),
        VARIATION_ENTRY(kIPHVideoTutorialDownloadFeature),
        VARIATION_ENTRY(kIPHVideoTutorialSearchFeature),
        VARIATION_ENTRY(kIPHExploreSitesTileFeature),
        VARIATION_ENTRY(kIPHFeedHeaderMenuFeature),
#elif defined(OS_IOS)
        VARIATION_ENTRY(kIPHBottomToolbarTipFeature),
        VARIATION_ENTRY(kIPHLongPressToolbarTipFeature),
        VARIATION_ENTRY(kIPHNewTabTipFeature),
        VARIATION_ENTRY(kIPHNewIncognitoTabTipFeature),
        VARIATION_ENTRY(kIPHBadgedReadingListFeature),
        VARIATION_ENTRY(kIPHBadgedTranslateManualTriggerFeature),
        VARIATION_ENTRY(kIPHDiscoverFeedHeaderFeature),
#elif defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
        VARIATION_ENTRY(kIPHDesktopTabGroupsNewGroupFeature),
        VARIATION_ENTRY(kIPHFocusModeFeature),
        VARIATION_ENTRY(kIPHGlobalMediaControls),
        VARIATION_ENTRY(kIPHLiveCaption),
        VARIATION_ENTRY(kIPHPasswordsAccountStorageFeature),
        VARIATION_ENTRY(kIPHReopenTabFeature),
        VARIATION_ENTRY(kIPHWebUITabStripFeature),
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)
};

#undef DEFINE_VARIATION_PARAM
#undef VARIATION_ENTRY

// Returns all the features that are in use for engagement tracking.
FeatureVector GetAllFeatures();

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
