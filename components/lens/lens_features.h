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

// Enables the Lens overlay translate button.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayTranslateButton);

// Enables the Lens overlay searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayContextualSearchbox);

// Enables the Lens overlay HaTS survey.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlaySurvey);

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
extern std::string GetLensOverlayActivityURL();

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

// Returns whether to use the tiered downscaling approach. If false, defaults to
// the normal since tier approach.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool LensOverlayUseTieredDownscaling();

// Returns whether or not to send a gen204 latency ping.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlaySendLatencyGen204();

// Returns whether or not to send task completion pings.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlaySendTaskCompletionGen204();

// Returns whether or not to send semantic event pings.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlaySendSemanticEventGen204();

// Returns the finch configured max image height for the Lens overlay feature
// when tiered downscaling approach is disabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxArea();

// Returns the finch configured max image height for the Lens overlay feature
// between Tiers 1 and 2. With the UI scaling, there is a possibility the image
// needs to be downscaled, but doesn't fit in a specific tier, which is when the
// image gets downscaled to this value. Corresponds to Tier 1.5 in
// go/lens-overlay-tiered-downscaling. This is also the value used if tier
// downscaling approach is disabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxHeight();

// Returns the finch configured max image width for the Lens overlay feature
// between Tiers 1 and 2. With the UI scaling, there is a possibility the image
// needs to be downscaled, but doesn't fit in a specific tier, which is when the
// image gets downscaled to this value. Corresponds to Tier 1.5 in
// go/lens-overlay-tiered-downscaling.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxWidth();

// Returns the finch configured max image area for the Lens overlay feature at
// Tier 1. Tier 1 is the lower resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxAreaTier1();

// Returns the finch configured max image height for the Lens overlay feature at
// Tier 1. Tier 1 is the lower resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxHeightTier1();

// Returns the finch configured max image width for the Lens overlay feature at
// Tier 1. Tier 1 is the lower resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxWidthTier1();

// Returns the finch configured max image area for the Lens overlay feature at
// Tier 2. Tier 2 is the middle tier resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxAreaTier2();

// Returns the finch configured max image height for the Lens overlay feature at
// Tier 2. Tier 2 is the middle tier resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxHeightTier2();

// Returns the finch configured max image width for the Lens overlay feature at
// Tier 2. Tier 2 is the middle tier resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxWidthTier2();

// Returns the finch configured max image area for the Lens overlay feature at
// Tier 3. Tier 3 is the highest resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxAreaTier3();

// Returns the finch configured max image height for the Lens overlay feature at
// Tier 3. Tier 3 is the highest resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxHeightTier3();

// Returns the finch configured max image width for the Lens overlay feature at
// Tier 3. Tier 3 is the highest resolution we downscale to.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageMaxWidthTier3();

// Returns the finch configured UI scaling factor that is used to decide what
// tier the captured screenshot should be downscaled to. A lower scaling factor
// threshold leads to more downscaling at that threshold.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayImageDownscaleUiScalingFactorThreshold();

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
// requests. This is sent in the video params urlparam.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseVideoContextForTextOnlyLensOverlayRequests();

// Returns whether to include the search context with text-only Lens Overlay
// requests. This is sent in the video params urlparam.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseVideoContextForMultimodalLensOverlayRequests();

// Returns whether to use the new optimized request flow which makes a request
// to get the cluster info prior to uploading any image or page content bytes.
// This also decouples sending the images and page content bytes in the same
// request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseOptimizedRequestFlow();

// Returns the finch configured endpoint URL for the cluster info request.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayClusterInfoEndpointUrl();

// Returns the max number of bytes to allow for content uploads.
COMPONENT_EXPORT(LENS_FEATURES)
extern uint32_t GetLensOverlayFileUploadLimitBytes();

// Returns whether to use the &vit=pdf param for the search request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UsePdfVitParam();

// Returns whether to use the &vit=wp param for the search request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseWebpageVitParam();

// Returns whether to include PDFs from the underlying page in the request to be
// used as page context.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UsePdfsAsContext();

// Returns whether to include the inner text from the underlying page in the
// request to be used as page context. This is for webpages and sends text
// equivalent to document.body.innerText.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseInnerTextAsContext();

// Returns whether to include the inner html from the underlying page in the
// request to be used as page context. Does nothing if UseInnerTextAsContext is
// enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseInnerHtmlAsContext();

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

// Returns whether to render the sparkling effect on the Lens overlay shimmer.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayShimmerSparklesEnabled();

// Returns whether to require that Google is the user's DSE (default search
// engine) for the Lens overlay feature to be enabled.
//
// NOTE: This should only be used for internal testing.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayGoogleDseRequired();

// Returns the finch configured loading image URL for the results in Lens
// Overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayResultsSearchLoadingURL(bool dark_mode);

// Returns the ideal height of the region that is created when a user taps
// rather than drags.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayTapRegionHeight();

// Returns the ideal width of the region that is created when a user taps
// rather than drags.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayTapRegionWidth();

// Returns whether to enable the image context menu entry point for Lens
// Overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseLensOverlayForImageSearch();

// Returns whether to enable the video context menu entry point for Lens
// Overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseLensOverlayForVideoFrameSearch();

// Returns whether to enable the find-in-page entry point.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsFindInPageEntryPointEnabled();

// Returns whether to enable the omnibox entry point.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsOmniboxEntryPointEnabled();

// True if the overlay entrypoint should suppress its label and be always
// visible in the omnibox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsOmniboxEntrypointAlwaysVisible();

// Returns whether or not to read the browser dark mode setting
// for Lens Overlay. If false, it will fall back to light mode.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseBrowserDarkModeSettingForLensOverlay();

// Returns whether dynamic theme detection based on the screenshot is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsDynamicThemeDetectionEnabled();

// Returns the min threshold for the fraction of the pixels with the extracted
// vibrant or dynamic color out of the total number of pixels in the screenshot.
COMPONENT_EXPORT(LENS_FEATURES)
extern double DynamicThemeMinPopulationPct();

// Returns the min threshold for the chroma of the extracted vibrant or dynamic
// color to be considered for matching to a set of candidate color palettes.
COMPONENT_EXPORT(LENS_FEATURES)
extern double DynamicThemeMinChroma();

// Returns whether or not to send the visual search interaction param with
// Lens text selection queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool SendVisualSearchInteractionParamForLensTextQueries();

// Returns the minimum intersection over union between region and text to serve
// as a threshold for triggering select text chip over region search.
COMPONENT_EXPORT(LENS_FEATURES)
extern double GetLensOverlaySelectTextOverRegionTriggerThreshold();

// Returns whether the shimmer should be rendered using Canvas2D or CSS Paint
// Api.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlayUseShimmerCanvas();

// Minimum area (in device-independent pixels) for significant regions to send
// with the screenshot.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlaySignificantRegionMinArea();

// Maximum number of significant regions to send with the screenshot. If
// negative, no maximum will be imposed.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayMaxSignificantRegions();

// Threshold for comparing equality of object bounding box and previously
// selected bounding box. Unit is proportion of the image dimensions.
COMPONENT_EXPORT(LENS_FEATURES)
extern double GetLensOverlayPostSelectionComparisonThreshold();

// The radius of the live page / underlying tab contents blur in pixels.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayLivePageBlurRadiusPixels();

// Enables our custom blur layer instead of that built into the ui::Layer.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlayUseCustomBlur();

// The radius of blur in pixels for the custom blur. This is separate from
// LivePageBlurRadiusPixels because the custom blur applies a lower blur since
// it is being applied to a downsampled image.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayCustomBlurBlurRadiusPixels();

// Sets the quality of the custom blur layer. This is a number between 0 and 1
// used to down sample the screenshot and apply the blur to less pixels. For
// example, a value of 0.5 on a viewport of 1000x500 will take a down sampled
// screenshot of 500x250 and blur that then upsampled instead of the blurring
// the entire viewport.
COMPONENT_EXPORT(LENS_FEATURES)
extern double GetLensOverlayCustomBlurQuality();

// The amount of times per second to update the background blur. This should be
// a value in Hertz. Meaning, a value of 30 will refresh the blur 30 times a
// second, while a value of 0.5 will update once every two seconds.
COMPONENT_EXPORT(LENS_FEATURES)
extern double GetLensOverlayCustomBlurRefreshRateHertz();

// The timeout set for every request from the browser to the server in
// milliseconds.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayServerRequestTimeout();

// Whether the error page is enabled on the lens overlay. This error page is
// visible when the full image request times out or when the user is offline. It
// prevents the user from interacting with the side panel results and searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlayEnableErrorPage();

// The value of the search companion query parameter `gsc` used in search URLs
// that are loaded in the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayGscQueryParamValue();

// Whether to allow the Lens Overlay in fullscreen without top Chrome. When this
// is disabled, Lens Overlay is only enabled if top chrome is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlayEnableInFullscreen();

// The corner radius in pixels for the vertex corners of the segmentation mask.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlaySegmentationMaskCornerRadius();

// Number identifying variant sets of strings to use in the find bar.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayFindBarStringsVariant();

// Whether to show the translate button in the Lens Overlay to allow translation
// of the screenshot of the page.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayTranslateButtonEnabled();

// Whether to show the copy as image context menu option.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayCopyAsImageEnabled();

// Whether to show the save as image context menu option.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlaySaveAsImageEnabled();

// Time to wait for Lens text response before displaying the selected region
// context menu, in milliseconds.
COMPONENT_EXPORT(LENS_FEATURES)
int GetLensOverlayImageContextMenuActionsTextReceivedTimeout();

// Whether to show the contextual searchbox in the Lens Overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayContextualSearchboxEnabled();

// Time delay for the results trigger of the Lens Overlay HaTS survey.
COMPONENT_EXPORT(LENS_FEATURES)
extern base::TimeDelta GetLensOverlaySurveyResultsTime();

}  // namespace lens::features

#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
