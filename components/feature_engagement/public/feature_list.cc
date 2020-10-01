// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_list.h"

#include "base/stl_util.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

namespace {
// Whenever a feature is added to |kAllFeatures|, it should also be added as
// DEFINE_VARIATION_PARAM in the header, and also added to the
// |kIPHDemoModeChoiceVariations| array.
const base::Feature* const kAllFeatures[] = {
    &kIPHDummyFeature,  // Ensures non-empty array for all platforms.
#if defined(OS_ANDROID)
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
    &kIPHContextualSearchTranslationEnableFeature,
    &kIPHContextualSearchWebSearchFeature,
    &kIPHContextualSearchPromoteTapFeature,
    &kIPHContextualSearchPromotePanelOpenFeature,
    &kIPHContextualSearchOptInFeature,
    &kIPHContextualSearchTappedButShouldLongpressFeature,
    &kIPHDownloadSettingsFeature,
    &kIPHDownloadInfoBarDownloadContinuingFeature,
    &kIPHDownloadInfoBarDownloadsAreFasterFeature,
    &kIPHEphemeralTabFeature,
    &kIPHFeedCardMenuFeature,
    &kIPHHomepagePromoCardFeature,
    &kIPHIdentityDiscFeature,
    &kIPHKeyboardAccessoryAddressFillingFeature,
    &kIPHKeyboardAccessoryBarSwipingFeature,
    &kIPHKeyboardAccessoryPasswordFillingFeature,
    &kIPHKeyboardAccessoryPaymentFillingFeature,
    &kIPHPreviewsOmniboxUIFeature,
    &kIPHQuietNotificationPromptsFeature,
    &kIPHTabGroupsQuicklyComparePagesFeature,
    &kIPHTabGroupsTapToSeeAnotherTabFeature,
    &kIPHTabGroupsYourTabsAreTogetherFeature,
    &kIPHTabGroupsDragAndDropFeature,
    &kIPHTranslateMenuButtonFeature,
    &kIPHVideoTutorialDownloadFeature,
    &kIPHVideoTutorialSearchFeature,
    &kIPHExploreSitesTileFeature,
    &kIPHFeedHeaderMenuFeature,
#endif  // defined(OS_ANDROID)
#if defined(OS_IOS)
    &kIPHBottomToolbarTipFeature,
    &kIPHLongPressToolbarTipFeature,
    &kIPHNewTabTipFeature,
    &kIPHNewIncognitoTabTipFeature,
    &kIPHBadgedReadingListFeature,
    &kIPHBadgedTranslateManualTriggerFeature,
    &kIPHDiscoverFeedHeaderFeature,
#endif  // defined(OS_IOS)
#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
    &kIPHDesktopTabGroupsNewGroupFeature,
    &kIPHFocusModeFeature,
    &kIPHGlobalMediaControlsFeature,
    &kIPHLiveCaptionFeature,
    &kIPHPasswordsAccountStorageFeature,
    &kIPHReopenTabFeature,
    &kIPHWebUITabStripFeature,
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)
};
}  // namespace

const char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + base::size(kAllFeatures));
}

}  // namespace feature_engagement
