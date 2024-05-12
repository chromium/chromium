// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace lens::features {

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

BASE_FEATURE(kEnableContextMenuInLensSidePanel,
             "EnableContextMenuInLensSidePanel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlay, "LensOverlay", base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kLensOverlayMinRamMb{&kLensOverlay, "min_ram_mb",
                                                   /*default=value=*/-1};
const base::FeatureParam<std::string> kHelpCenterUrl{
    &kLensOverlay, "help-center-url",
    "https://support.google.com/chrome?p=search_from_page"};
const base::FeatureParam<std::string> kResultsSearchUrl{
    &kLensOverlay, "results-search-url", "https://www.google.com/search"};
const base::FeatureParam<int> kLensOverlayScreenshotRenderQuality{
    &kLensOverlay, "overlay-screenshot-render-quality", 90};
const base::FeatureParam<int> kLensOverlayImageCompressionQuality{
    &kLensOverlay, "image-compression-quality", 40};
const base::FeatureParam<int> kLensOverlayImageMaxArea{
    &kLensOverlay, "image-dimensions-max-area", 1300000};
const base::FeatureParam<int> kLensOverlayImageMaxHeight{
    &kLensOverlay, "image-dimensions-max-height", 1500};
const base::FeatureParam<int> kLensOverlayImageMaxWidth{
    &kLensOverlay, "image-dimensions-max-width", 1500};
const base::FeatureParam<bool> kLensOverlayDebuggingMode{
    &kLensOverlay, "debugging-mode", false};
const base::FeatureParam<int> kLensOverlayVerticalTextMargin{
    &kLensOverlay, "text-vertical-margin", 4};
const base::FeatureParam<int> kLensOverlayHorizontalTextMargin{
    &kLensOverlay, "text-horizontal-margin", 4};
const base::FeatureParam<bool> kLensOverlaySearchBubble{&kLensOverlay,
                                                        "search-bubble", false};
const base::FeatureParam<bool> kLensOverlayEnableShimmer{
    &kLensOverlay, "enable-shimmer", true};
const base::FeatureParam<bool> kLensOverlaySelectionDraggingEnabled{
    &kLensOverlay, "enable-selection-dragging", false};
const base::FeatureParam<std::string> kResultsSearchLoadingUrl{
    &kLensOverlay, "results-search-loading-url",
    "https://www.gstatic.com/lens/chrome/"
    "lens_overlay_sidepanel_results_ghostloader_light-"
    "71af0ff0f00a1a03d3fe8abad71a2665.svg"};

const base::FeatureParam<bool> kLensOverlayGoogleDseRequired{
    &kLensOverlay, "google-dse-required", true};

constexpr base::FeatureParam<std::string> kLensOverlayEndpointUrl{
    &kLensOverlay, "endpoint-url",
    "https://lensfrontend-pa.googleapis.com/v1/crupload"};

constexpr base::FeatureParam<bool> kUseOauthForLensOverlayRequests{
    &kLensOverlay, "use-oauth-for-requests", true};

constexpr base::FeatureParam<int> kLensOverlayClusterInfoLifetimeSeconds{
    &kLensOverlay, "cluster-info-lifetime-seconds", 600};

constexpr base::FeatureParam<bool>
    kUseSearchContextForTextOnlyLensOverlayRequests{
        &kLensOverlay, "use-search-context-for-text-only-requests", false};

constexpr base::FeatureParam<int> kLensOverlayTapRegionHeight{
    &kLensOverlay, "tap-region-height", 300};
constexpr base::FeatureParam<int> kLensOverlayTapRegionWidth{
    &kLensOverlay, "tap-region-width", 300};

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

constexpr base::FeatureParam<int> kMaxAreaForImageSearch{
    &kLensImageCompression, "dimensions-max-area", 1000000};

constexpr base::FeatureParam<int> kMaxPixelsForImageSearch{
    &kLensImageCompression, "dimensions-max-pixels", 1000};

const base::FeatureParam<bool> kEnableLensFullscreenSearch{
    &kLensSearchOptimizations, "enable-lens-fullscreen-search", false};

bool GetEnableLatencyLogging() {
  return base::FeatureList::IsEnabled(kEnableLatencyLogging) &&
         base::FeatureList::IsEnabled(kLensStandalone);
}

int GetMaxAreaForImageSearch() {
  return kMaxAreaForImageSearch.Get();
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

bool IsLensRegionSearchStaticPageEnabled() {
  return base::FeatureList::IsEnabled(kLensRegionSearchStaticPage);
}

bool GetEnableContextMenuInLensSidePanel() {
  return base::FeatureList::IsEnabled(kEnableContextMenuInLensSidePanel);
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

bool IsLensOverlayEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlay);
}

std::string GetLensOverlayHelpCenterURL() {
  return kHelpCenterUrl.Get();
}

int GetLensOverlayMinRamMb() {
  return kLensOverlayMinRamMb.Get();
}

std::string GetLensOverlayResultsSearchURL() {
  return kResultsSearchUrl.Get();
}

int GetLensOverlayImageCompressionQuality() {
  return kLensOverlayImageCompressionQuality.Get();
}

int GetLensOverlayScreenshotRenderQuality() {
  return kLensOverlayScreenshotRenderQuality.Get();
}

int GetLensOverlayImageMaxArea() {
  return kLensOverlayImageMaxArea.Get();
}

int GetLensOverlayImageMaxHeight() {
  return kLensOverlayImageMaxHeight.Get();
}

int GetLensOverlayImageMaxWidth() {
  return kLensOverlayImageMaxWidth.Get();
}

std::string GetLensOverlayEndpointURL() {
  return kLensOverlayEndpointUrl.Get();
}

bool IsLensOverlayDebuggingEnabled() {
  return kLensOverlayDebuggingMode.Get();
}

bool UseOauthForLensOverlayRequests() {
  return kUseOauthForLensOverlayRequests.Get();
}

int GetLensOverlayClusterInfoLifetimeSeconds() {
  return kLensOverlayClusterInfoLifetimeSeconds.Get();
}

bool UseSearchContextForTextOnlyLensOverlayRequests() {
  return kUseSearchContextForTextOnlyLensOverlayRequests.Get();
}

int GetLensOverlayVerticalTextMargin() {
  return kLensOverlayVerticalTextMargin.Get();
}

int GetLensOverlayHorizontalTextMargin() {
  return kLensOverlayHorizontalTextMargin.Get();
}

bool IsLensOverlaySearchBubbleEnabled() {
  return kLensOverlaySearchBubble.Get();
}

bool IsLensOverlayShimmerEnabled() {
  return kLensOverlayEnableShimmer.Get();
}

bool IsLensOverlaySelectionDraggingEnabled() {
  return kLensOverlaySelectionDraggingEnabled.Get();
}

bool IsLensOverlayGoogleDseRequired() {
  return kLensOverlayGoogleDseRequired.Get();
}

std::string GetLensOverlayResultsSearchLoadingURL() {
  return kResultsSearchLoadingUrl.Get();
}

int GetLensOverlayTapRegionHeight() {
  return kLensOverlayTapRegionHeight.Get();
}

int GetLensOverlayTapRegionWidth() {
  return kLensOverlayTapRegionWidth.Get();
}

}  // namespace lens::features
