// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_

#include <vector>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/feature_engagement/buildflags.h"
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
        arraysize(base_feature##Variation), nullptr                  \
  }

// Defines a flags_ui::FeatureEntry::FeatureParam for each feature.
DEFINE_VARIATION_PARAM(kIPHDummyFeature, "IPH_Dummy");
#if defined(OS_ANDROID)
DEFINE_VARIATION_PARAM(kIPHDataSaverDetailFeature, "IPH_DataSaverDetail");
DEFINE_VARIATION_PARAM(kIPHDataSaverPreviewFeature, "IPH_DataSaverPreview");
DEFINE_VARIATION_PARAM(kIPHDownloadHomeFeature, "IPH_DownloadHome");
DEFINE_VARIATION_PARAM(kIPHDownloadPageFeature, "IPH_DownloadPage");
DEFINE_VARIATION_PARAM(kIPHDownloadPageScreenshotFeature,
                       "IPH_DownloadPageScreenshot");
DEFINE_VARIATION_PARAM(kIPHChromeDuetFeature, "IPH_ChromeDuet");
DEFINE_VARIATION_PARAM(kIPHChromeHomeExpandFeature, "IPH_ChromeHomeExpand");
DEFINE_VARIATION_PARAM(kIPHChromeHomePullToRefreshFeature,
                       "IPH_ChromeHomePullToRefresh");
DEFINE_VARIATION_PARAM(kIPHMediaDownloadFeature, "IPH_MediaDownload");
DEFINE_VARIATION_PARAM(kIPHContextualSearchWebSearchFeature,
                       "IPH_ContextualSearchWebSearch");
DEFINE_VARIATION_PARAM(kIPHContextualSearchPromoteTapFeature,
                       "IPH_ContextualSearchPromoteTap");
DEFINE_VARIATION_PARAM(kIPHContextualSearchPromotePanelOpenFeature,
                       "IPH_ContextualSearchPromotePanelOpen");
DEFINE_VARIATION_PARAM(kIPHContextualSearchOptInFeature,
                       "IPH_ContextualSearchOptIn");
DEFINE_VARIATION_PARAM(kIPHContextualSuggestionsFeature,
                       "IPH_ContextualSuggestions");
DEFINE_VARIATION_PARAM(kIPHDownloadSettingsFeature, "IPH_DownloadSettings");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadContinuingFeature,
                       "IPH_DownloadInfoBarDownloadContinuing");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadsAreFasterFeature,
                       "IPH_DownloadInfoBarDownloadsAreFaster");
DEFINE_VARIATION_PARAM(kIPHHomePageButtonFeature, "IPH_HomePageButton");
DEFINE_VARIATION_PARAM(kIPHHomepageTileFeature, "IPH_HomepageTile");
DEFINE_VARIATION_PARAM(kIPHNewTabPageButtonFeature, "IPH_NewTabPageButton");
DEFINE_VARIATION_PARAM(kIPHPreviewsOmniboxUIFeature, "IPH_PreviewsOmniboxUI");
#endif  // defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
DEFINE_VARIATION_PARAM(kIPHBookmarkFeature, "IPH_Bookmark");
DEFINE_VARIATION_PARAM(kIPHIncognitoWindowFeature, "IPH_IncognitoWindow");
DEFINE_VARIATION_PARAM(kIPHNewTabFeature, "IPH_NewTab");
DEFINE_VARIATION_PARAM(kIPHReopenTabFeature, "IPH_ReopenTab");
#endif  // BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
#if defined(OS_IOS)
DEFINE_VARIATION_PARAM(kIPHBottomToolbarTipFeature, "IPH_BottomToolbarTip");
DEFINE_VARIATION_PARAM(kIPHLongPressToolbarTipFeature,
                       "IPH_LongPressToolbarTip");
DEFINE_VARIATION_PARAM(kIPHNewTabTipFeature, "IPH_NewTabTip");
DEFINE_VARIATION_PARAM(kIPHNewIncognitoTabTipFeature, "IPH_NewIncognitoTabTip");
DEFINE_VARIATION_PARAM(kIPHBadgedReadingListFeature, "IPH_BadgedReadingList");
#endif  // defined(OS_IOS)

}  // namespace

// Defines the array of which features should be listed in the chrome://flags
// UI to be able to select them alone for demo-mode. The features listed here
// are possible to enable on their own in demo mode.
constexpr flags_ui::FeatureEntry::FeatureVariation
    kIPHDemoModeChoiceVariations[] = {
#if defined(OS_ANDROID)
        VARIATION_ENTRY(kIPHDataSaverDetailFeature),
        VARIATION_ENTRY(kIPHDataSaverPreviewFeature),
        VARIATION_ENTRY(kIPHDownloadHomeFeature),
        VARIATION_ENTRY(kIPHDownloadPageFeature),
        VARIATION_ENTRY(kIPHDownloadPageScreenshotFeature),
        VARIATION_ENTRY(kIPHChromeDuetFeature),
        VARIATION_ENTRY(kIPHChromeHomeExpandFeature),
        VARIATION_ENTRY(kIPHChromeHomePullToRefreshFeature),
        VARIATION_ENTRY(kIPHMediaDownloadFeature),
        VARIATION_ENTRY(kIPHContextualSearchWebSearchFeature),
        VARIATION_ENTRY(kIPHContextualSearchPromoteTapFeature),
        VARIATION_ENTRY(kIPHContextualSearchPromotePanelOpenFeature),
        VARIATION_ENTRY(kIPHContextualSearchOptInFeature),
        VARIATION_ENTRY(kIPHContextualSuggestionsFeature),
        VARIATION_ENTRY(kIPHDownloadSettingsFeature),
        VARIATION_ENTRY(kIPHDownloadInfoBarDownloadContinuingFeature),
        VARIATION_ENTRY(kIPHDownloadInfoBarDownloadsAreFasterFeature),
        VARIATION_ENTRY(kIPHHomePageButtonFeature),
        VARIATION_ENTRY(kIPHHomepageTileFeature),
        VARIATION_ENTRY(kIPHNewTabPageButtonFeature),
        VARIATION_ENTRY(kIPHPreviewsOmniboxUIFeature),
#elif BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
        VARIATION_ENTRY(kIPHBookmarkFeature),
        VARIATION_ENTRY(kIPHIncognitoWindowFeature),
        VARIATION_ENTRY(kIPHNewTabFeature),
        VARIATION_ENTRY(kIPHReopenTabFeature),
#elif defined(OS_IOS)
        VARIATION_ENTRY(kIPHBottomToolbarTipFeature),
        VARIATION_ENTRY(kIPHLongPressToolbarTipFeature),
        VARIATION_ENTRY(kIPHNewTabTipFeature),
        VARIATION_ENTRY(kIPHNewIncognitoTabTipFeature),
        VARIATION_ENTRY(kIPHBadgedReadingListFeature),
#else
        VARIATION_ENTRY(kIPHDummyFeature),  // Ensures non-empty array.
#endif
};

#undef DEFINE_VARIATION_PARAM
#undef VARIATION_ENTRY

// Returns all the features that are in use for engagement tracking.
FeatureVector GetAllFeatures();

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
