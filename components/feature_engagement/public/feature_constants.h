// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace feature_engagement {

// A feature for enabling a demonstration mode for In-Product Help (IPH).
extern const base::Feature kIPHDemoMode;

// A feature to ensure all arrays can contain at least one feature.
extern const base::Feature kIPHDummyFeature;

#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
extern const base::Feature kIPHDesktopTabGroupsNewGroupFeature;
extern const base::Feature kIPHFocusModeFeature;
extern const base::Feature kIPHGlobalMediaControlsFeature;
extern const base::Feature kIPHLiveCaptionFeature;
extern const base::Feature kIPHPasswordsAccountStorageFeature;
extern const base::Feature kIPHReopenTabFeature;
extern const base::Feature kIPHWebUITabStripFeature;
extern const base::Feature kIPHDesktopSnoozeFeature;
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

// All the features declared for Android below that are also used in Java,
// should also be declared in:
// org.chromium.components.feature_engagement.FeatureConstants.
#if defined(OS_ANDROID)
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
extern const base::Feature kIPHDownloadSettingsFeature;
extern const base::Feature kIPHDownloadInfoBarDownloadContinuingFeature;
extern const base::Feature kIPHDownloadInfoBarDownloadsAreFasterFeature;
extern const base::Feature kIPHEphemeralTabFeature;
extern const base::Feature kIPHFeedCardMenuFeature;
extern const base::Feature kIPHHomePageButtonFeature;
extern const base::Feature kIPHHomepageTileFeature;
extern const base::Feature kIPHHomepagePromoCardFeature;
extern const base::Feature kIPHIdentityDiscFeature;
extern const base::Feature kIPHKeyboardAccessoryAddressFillingFeature;
extern const base::Feature kIPHKeyboardAccessoryBarSwipingFeature;
extern const base::Feature kIPHKeyboardAccessoryPasswordFillingFeature;
extern const base::Feature kIPHKeyboardAccessoryPaymentFillingFeature;
extern const base::Feature kIPHNewTabPageButtonFeature;
extern const base::Feature kIPHPreviewsOmniboxUIFeature;
extern const base::Feature kIPHQuietNotificationPromptsFeature;
extern const base::Feature kIPHTabGroupsQuicklyComparePagesFeature;
extern const base::Feature kIPHTabGroupsTapToSeeAnotherTabFeature;
extern const base::Feature kIPHTabGroupsYourTabsAreTogetherFeature;
extern const base::Feature kIPHTabGroupsDragAndDropFeature;
extern const base::Feature kIPHTranslateMenuButtonFeature;
extern const base::Feature kIPHVideoTutorialDownloadFeature;
extern const base::Feature kIPHVideoTutorialSearchFeature;
extern const base::Feature kIPHExploreSitesTileFeature;
extern const base::Feature kIPHFeedHeaderMenuFeature;
extern const base::Feature kIPHChromeReengagementNotification1Feature;
extern const base::Feature kIPHChromeReengagementNotification2Feature;
extern const base::Feature kIPHChromeReengagementNotification3Feature;
#endif  // defined(OS_ANDROID)

#if defined(OS_IOS)
extern const base::Feature kIPHBottomToolbarTipFeature;
extern const base::Feature kIPHLongPressToolbarTipFeature;
extern const base::Feature kIPHNewTabTipFeature;
extern const base::Feature kIPHNewIncognitoTabTipFeature;
extern const base::Feature kIPHBadgedReadingListFeature;
extern const base::Feature kIPHBadgedTranslateManualTriggerFeature;
extern const base::Feature kIPHDiscoverFeedHeaderFeature;
#endif  // defined(OS_IOS)

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_CONSTANTS_H_
