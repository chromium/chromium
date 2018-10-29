/// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_constants.h"

#include "components/feature_engagement/buildflags.h"

namespace feature_engagement {

const base::Feature kIPHDemoMode{"IPH_DemoMode",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIPHDummyFeature{"IPH_Dummy",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
const base::Feature kIPHDataSaverDetailFeature{
    "IPH_DataSaverDetail", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDataSaverPreviewFeature{
    "IPH_DataSaverPreview", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadHomeFeature{"IPH_DownloadHome",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadPageFeature{"IPH_DownloadPage",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadPageScreenshotFeature{
    "IPH_DownloadPageScreenshot", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeDuetFeature{"IPH_ChromeDuet",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeHomeExpandFeature{
    "IPH_ChromeHomeExpand", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHChromeHomePullToRefreshFeature{
    "IPH_ChromeHomePullToRefresh", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHMediaDownloadFeature{"IPH_MediaDownload",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchWebSearchFeature{
    "IPH_ContextualSearchWebSearch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchPromoteTapFeature{
    "IPH_ContextualSearchPromoteTap", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchPromotePanelOpenFeature{
    "IPH_ContextualSearchPromotePanelOpen", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSearchOptInFeature{
    "IPH_ContextualSearchOptIn", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHContextualSuggestionsFeature{
    "IPH_ContextualSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadSettingsFeature{
    "IPH_DownloadSettings", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadInfoBarDownloadContinuingFeature{
    "IPH_DownloadInfoBarDownloadContinuing", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHDownloadInfoBarDownloadsAreFasterFeature{
    "IPH_DownloadInfoBarDownloadsAreFaster", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHHomePageButtonFeature{
    "IPH_HomePageButton", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHHomepageTileFeature{"IPH_HomepageTile",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHNewTabPageButtonFeature{
    "IPH_NewTabPageButton", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHPreviewsOmniboxUIFeature{
    "IPH_PreviewsOmniboxUI", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
const base::Feature kIPHBookmarkFeature{"IPH_Bookmark",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHIncognitoWindowFeature{
    "IPH_IncognitoWindow", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHNewTabFeature{"IPH_NewTab",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kIPHReopenTabFeature{"IPH_ReopenTab",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(ENABLE_DESKTOP_IPH)

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
    "IPH_BadgedReadingList", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_IOS)

}  // namespace feature_engagement
