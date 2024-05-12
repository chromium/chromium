// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_FEATURES_H_
#define COMPONENTS_LENS_LENS_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace lens::features {

// Enables context menu search by image sending to the Lens homepage.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensStandalone);

// Feature that controls the compression of images before they are sent to Lens.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensImageCompression);

// Enables a variety of changes aimed to improve user's engagement with current
// Lens features.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchOptimizations);

// Enables Latency logging for the LensStandalone feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableLatencyLogging);

// Enable keyboard shortcut for the Lens Region Search feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableRegionSearchKeyboardShortcut);

// Enables context menu option for translating image feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableImageTranslate);

// Enables the image search side panel experience for third party default search
// engines
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableImageSearchSidePanelFor3PDse);

// Enables launching the region search experience in a new tab with WebUI.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensRegionSearchStaticPage);

// Enables the context menu in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(EnableContextMenuInLensSidePanel);

// Enables the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlay);

// The base URL for Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<std::string> kHomepageURLForLens;

// Enable Lens HTML redirect fix.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableLensHtmlRedirectFix;

// Enable Lens loading state removal on
// DocumentOnLoadCompletedInPrimaryMainFrame.
// TODO(crbug.com/40916154): Clean up unused listeners and flags after
// determining which ones we want to listen to for server-side rendering
// backends.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool>
    kDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame;

// Enable Lens loading state removal on DomContentLoaded.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kDismissLoadingStateOnDomContentLoaded;

// Enable Lens loading state removal on DidFinishNavigation.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kDismissLoadingStateOnDidFinishNavigation;

// Enable Lens loading state removal on NavigationEntryCommitted.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool>
    kDismissLoadingStateOnNavigationEntryCommitted;

// Enable Lens loading state removal on DidFinishLoad.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kDismissLoadingStateOnDidFinishLoad;

// Enable Lens loading state removal on PrimaryPageChanged.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kDismissLoadingStateOnPrimaryPageChanged;

// Enables Lens fullscreen search on Desktop platforms.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableFullscreenSearch;

// Enables Latency logging for the LensStandalone feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLatencyLogging();

// Returns whether the image search side panel is supported for third party
// default search engines
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableImageSearchUnifiedSidePanelFor3PDse();

// Returns the max area for the image to be sent to Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetMaxAreaForImageSearch();

// Returns the max pixel width/height for the image to be sent to Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetMaxPixelsForImageSearch();

// The URL for the Lens home page.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetHomepageURLForLens();

// Returns whether to apply fix for HTML redirects.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLensHtmlRedirectFix();

// Returns whether to remove the Lens side panel loading state in the
// OnDocumentOnLoadCompletedInPrimaryMainFrame web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnDocumentOnLoadCompletedInPrimaryMainFrame();

// Returns whether to remove the Lens side panel loading state in the
// DOMContentLoaded web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnDomContentLoaded();

// Returns whether to remove the Lens side panel loading state in the
// DidFinishNavigation web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnDidFinishNavigation();

// Returns whether to remove the Lens side panel loading state in the
// NavigationEntryCommitted web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnNavigationEntryCommitted();

// Returns whether to remove the Lens side panel loading state in the
// DidFinishLoad web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnDidFinishLoad();

// Returns whether to remove the Lens side panel loading state in the
// PrimaryPageChanged web contents observer callback.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetDismissLoadingStateOnPrimaryPageChanged();

// Returns whether Lens fullscreen search is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensFullscreenSearchEnabled();

// Returns whether the Lens side panel is enabled for image search.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSidePanelEnabled();

// Returns whether the Search Image button in the Chrome Screenshot Sharing
// feature is enabled
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensInScreenshotSharingEnabled();

// Returns whether we should use a WebUI static page for region search.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensRegionSearchStaticPageEnabled();

// Returns whether to enable the context menu in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableContextMenuInLensSidePanel();

// The URL for the Lens ping.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensPingURL();

// Returns whether or not the Lens ping should be done sequentially.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensPingIsSequential();

// Returns whether to issue Lens preconnect requests when the
// context menu item is shown.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetShouldIssuePreconnectForLens();

// Returns the preconnect url to use for when the context menu item
// is shown.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetPreconnectKeyForLens();

// Returns whether to start a Spare Render process when the context menu item
// is shown.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetShouldIssueProcessPrewarmingForLens();

// Returns whether the kLensOverlay Feature is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayEnabled();

// Returns the finch configured help center URL for lens permission modal.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayHelpCenterURL();

// Returns the minimum amount of physical memory required to enable the Lens
// overlay feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayMinRamMb();

// Returns the finch configured results search URL to use as base for queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayResultsSearchURL();

// Returns the finch configured image compression quality for the Lens overlay
// feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageCompressionQuality();

// Returns the finch configured image compression quality for the full
// screenshot being used for the lens overlay feature. This is different than
// the "image" compression quality because it is stays on the client to be
// displayed on the overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayScreenshotRenderQuality();

// Returns the finch configured max image area for the Lens overlay feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxArea();

// Returns the finch configured max image height for the Lens overlay feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxHeight();

// Returns the finch configured max image width for the Lens overlay feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxWidth();

// Returns the finch configured endpoint URL for the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayEndpointURL();

// Returns whether to highlight text and object bounding boxes for debugging.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayDebuggingEnabled();

// Returns whether to use oauth for signed in requests to the endpoint.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseOauthForLensOverlayRequests();

// Returns the time before the Lens overlay cluster info is invalid, in seconds.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayClusterInfoLifetimeSeconds();

// Returns whether to include the search context with text-only Lens Overlay
// requests.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseSearchContextForTextOnlyLensOverlayRequests();

// Returns the margin in pixels to add to the top and bottom of word bounding
// boxes.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayVerticalTextMargin();

// Returns the margin in pixels to add to the left and right of word bounding
// boxes.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayHorizontalTextMargin();

// Returns whether to show the lens overlay search bubble.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlaySearchBubbleEnabled();

// Returns whether to render the Lens overlay shimmer.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayShimmerEnabled();

// Returns whether to allow dragging the Lens overlay selection box.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlaySelectionDraggingEnabled();

// Returns whether to require that Google is the user's DSE (default search
// engine) for the Lens overlay feature to be enabled.
//
// NOTE: This should only be used for internal testing.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayGoogleDseRequired();

// Returns the finch configured loading image URL for the results in Lens
// Overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayResultsSearchLoadingURL();

// Returns the ideal height of the region that is created when a user taps
// rather than drags.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayTapRegionHeight();

// Returns the ideal width of the region that is created when a user taps
// rather than drags.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayTapRegionWidth();
}  // namespace lens::features

#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
