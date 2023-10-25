// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace lens {
namespace features {

BASE_FEATURE(kLensStandalone,
             "LensStandalone",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensImageCompression,
             "LensImageCompression",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensSearchOptimizations,
             "LensSearchOptimizations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLatencyLogging,
             "LensImageLatencyLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableRegionSearchKeyboardShortcut,
             "LensEnableRegionSearchKeyboardShortcut",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableImageTranslate,
             "LensEnableImageTranslate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableImageSearchSidePanelFor3PDse,
             "EnableImageSearchSidePanelFor3PDse",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensRegionSearchStaticPage,
             "LensRegionSearchStaticPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensImageFormatOptimizations,
             "LensImageFormatOptimizations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableContextMenuInLensSidePanel,
             "EnableContextMenuInLensSidePanel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableLensPing,
             "EnableLensPing",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kHomepageURLForLens{
    &kLensStandalone, "lens-homepage-url", "https://lens.google.com/v3/"};

constexpr base::FeatureParam<bool> kEnableLensHtmlRedirectFix{
    &kLensStandalone, "lens-html-redirect-fix", false};

constexpr base::FeatureParam<bool>
    kDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame{
        &kLensStandalone,
        "dismiss-loading-state-on-document-on-load-completed-in-primary-main-"
        "frame",
        false};

constexpr base::FeatureParam<bool> kDismissLoadingStateOnDomContentLoaded{
    &kLensStandalone, "dismiss-loading-state-on-dom-content-loaded", false};

constexpr base::FeatureParam<bool> kDismissLoadingStateOnDidFinishNavigation{
    &kLensStandalone, "dismiss-loading-state-on-did-finish-navigation", false};

constexpr base::FeatureParam<bool>
    kDismissLoadingStateOnNavigationEntryCommitted{
        &kLensStandalone, "dismiss-loading-state-on-navigation-entry-committed",
        true};

constexpr base::FeatureParam<bool> kShouldIssuePreconnectForLens{
    &kLensStandalone, "lens-issue-preconnect", true};

constexpr base::FeatureParam<std::string> kPreconnectKeyForLens{
    &kLensStandalone, "lens-preconnect-key", "https://google.com"};

constexpr base::FeatureParam<bool> kShouldIssueProcessPrewarmingForLens{
    &kLensStandalone, "lens-issue-process-prewarming", true};

constexpr base::FeatureParam<bool> kDismissLoadingStateOnDidFinishLoad{
    &kLensStandalone, "dismiss-loading-state-on-did-finish-load", false};

constexpr base::FeatureParam<bool> kDismissLoadingStateOnPrimaryPageChanged{
    &kLensStandalone, "dismiss-loading-state-on-primary-page-changed", false};

constexpr base::FeatureParam<int> kMaxPixelsForRegionSearch{
    &kLensImageCompression, "region-search-dimensions-max-pixels", 1000};

constexpr base::FeatureParam<int> kMaxAreaForRegionSearch{
    &kLensImageCompression, "region-search-dimensions-max-area", 1000000};

constexpr base::FeatureParam<int> kMaxPixelsForImageSearch{
    &kLensImageCompression, "dimensions-max-pixels", 1000};

const base::FeatureParam<bool> kEnableLensFullscreenSearch{
    &kLensSearchOptimizations, "enable-lens-fullscreen-search", false};

const base::FeatureParam<bool> kLensContextMenuUseAlternateText{
    &kLensSearchOptimizations, "use-lens-context-menu-alternate-text", false};

const base::FeatureParam<bool> kUseWebpInImageSearch{
    &kLensImageFormatOptimizations, "use-webp-image-search", true};

const base::FeatureParam<int> kEncodingQualityImageSearch{
    &kLensImageFormatOptimizations, "encoding-quality-image-search", 45};

const base::FeatureParam<bool> kUseWebpInRegionSearch{
    &kLensImageFormatOptimizations, "use-webp-region-search", false};

const base::FeatureParam<bool> kUseJpegInRegionSearch{
    &kLensImageFormatOptimizations, "use-jpeg-region-search", true};

const base::FeatureParam<int> kEncodingQualityRegionSearch{
    &kLensImageFormatOptimizations, "encoding-quality-region-search", 40};

constexpr base::FeatureParam<std::string> kLensPingURL{
    &kEnableLensPing, "lens-ping-url",
    "https://lens.google.com/_/LensWebStandaloneUi/gen204/"};

const base::FeatureParam<bool> kPingLensSequentially{
    &kEnableLensPing, "ping-lens-sequentially", true};

bool GetEnableLatencyLogging() {
  return base::FeatureList::IsEnabled(kEnableLatencyLogging) &&
         base::FeatureList::IsEnabled(kLensStandalone);
}

int GetMaxPixelsForRegionSearch() {
  return kMaxPixelsForRegionSearch.Get();
}

int GetMaxAreaForRegionSearch() {
  return kMaxAreaForRegionSearch.Get();
}

int GetMaxPixelsForImageSearch() {
  return kMaxPixelsForImageSearch.Get();
}

std::string GetHomepageURLForLens() {
  return kHomepageURLForLens.Get();
}

bool GetEnableLensHtmlRedirectFix() {
  return kEnableLensHtmlRedirectFix.Get();
}

bool GetDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame() {
  return kDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame.Get();
}

bool GetDismissLoadingStateOnDomContentLoaded() {
  return kDismissLoadingStateOnDomContentLoaded.Get();
}

bool GetDismissLoadingStateOnDidFinishNavigation() {
  return kDismissLoadingStateOnDidFinishNavigation.Get();
}

bool GetDismissLoadingStateOnNavigationEntryCommitted() {
  return kDismissLoadingStateOnNavigationEntryCommitted.Get();
}

bool GetDismissLoadingStateOnDidFinishLoad() {
  return kDismissLoadingStateOnDidFinishLoad.Get();
}

bool GetDismissLoadingStateOnPrimaryPageChanged() {
  return kDismissLoadingStateOnPrimaryPageChanged.Get();
}

bool GetEnableImageSearchUnifiedSidePanelFor3PDse() {
  return base::FeatureList::IsEnabled(kEnableImageSearchSidePanelFor3PDse);
}

bool IsLensFullscreenSearchEnabled() {
  return base::FeatureList::IsEnabled(kLensStandalone) &&
         base::FeatureList::IsEnabled(kLensSearchOptimizations) &&
         kEnableLensFullscreenSearch.Get();
}

bool IsLensSidePanelEnabled() {
  return base::FeatureList::IsEnabled(kLensStandalone);
}

bool IsLensSidePanelEnabledForRegionSearch() {
  return IsLensSidePanelEnabled() && !IsLensFullscreenSearchEnabled();
}

bool IsLensRegionSearchStaticPageEnabled() {
  return base::FeatureList::IsEnabled(kLensRegionSearchStaticPage);
}

bool UseLensContextMenuItemAlternateText() {
  return base::FeatureList::IsEnabled(kLensStandalone) &&
         base::FeatureList::IsEnabled(kLensSearchOptimizations) &&
         kLensContextMenuUseAlternateText.Get();
}

bool IsWebpForImageSearchEnabled() {
  return base::FeatureList::IsEnabled(kLensImageFormatOptimizations) &&
         kUseWebpInImageSearch.Get();
}

int GetImageSearchEncodingQuality() {
  return kEncodingQualityImageSearch.Get();
}

bool IsWebpForRegionSearchEnabled() {
  return base::FeatureList::IsEnabled(kLensImageFormatOptimizations) &&
         kUseWebpInRegionSearch.Get();
}

bool IsJpegForRegionSearchEnabled() {
  return base::FeatureList::IsEnabled(kLensImageFormatOptimizations) &&
         kUseJpegInRegionSearch.Get();
}

int GetRegionSearchEncodingQuality() {
  return kEncodingQualityRegionSearch.Get();
}

bool GetEnableContextMenuInLensSidePanel() {
  return base::FeatureList::IsEnabled(kEnableContextMenuInLensSidePanel);
}

bool GetEnableLensPing() {
  return base::FeatureList::IsEnabled(kEnableLensPing);
}

std::string GetLensPingURL() {
  return kLensPingURL.Get();
}

bool GetLensPingIsSequential() {
  return kPingLensSequentially.Get();
}

bool GetShouldIssuePreconnectForLens() {
  return kShouldIssuePreconnectForLens.Get();
}

std::string GetPreconnectKeyForLens() {
  return kPreconnectKeyForLens.Get();
}

bool GetShouldIssueProcessPrewarmingForLens() {
  return kShouldIssueProcessPrewarmingForLens.Get();
}

}  // namespace features
}  // namespace lens
