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

// Enables the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlay);

// Enables the Lens overlay translate button.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayTranslateButton);

// Enables the Lens overlay translate button.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayTranslateLanguages);

// Enables the Lens overlay image context menu actions.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayImageContextMenuActions);

// Enables the Lens overlay searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayContextualSearchbox);

// Enables the migration for Lens overlay suggestions URL params, independent
// of the CSB feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlaySuggestionsMigration);

// Enables the Lens overlay optimizations.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayLatencyOptimizations);

// Enables the Lens overlay routing info optimizations.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayRoutingInfo);

// Enables the Lens overlay HaTS survey.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlaySurvey);

// Enables the Lens overlay side panel open in new tab option.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlaySidePanelOpenInNewTab);

// Enables the Lens overlay visual selection updates.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayVisualSelectionUpdates);

// Enables the Lens overlay updated client context.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayUpdatedClientContext);

// Enables the Lens Overlay omnibox entry point.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayOmniboxEntryPoint);

// Enables uploading chunking for the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayUploadChunking);

// Enables a new feedback entrypoint in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchSidePanelNewFeedback);

// Enables recontextualizing on each query for the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayRecontextualizeOnQuery);

// Enables the Lens overlay simplified selection flow.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayCornerSliders);

// Enables the protected error page in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchProtectedPage);

// Enables the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayEduActionChip);

// Enables keyboard selection in the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayKeyboardSelection);

// Use alternate appearance for permission bubble.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayPermissionBubbleAlt);

// Enables the search not found on page toast when a user clicks a citation for
// the current page they are viewing but the text was not found.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchNotFoundOnPageToast);

// Enables straight to SRP flows are enabled in the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayStraightToSrp);

// Enables AIM follow ups with the Lens overlay results side panel globally.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchAimM3);

// Enables AIM follow ups with the Lens overlay results side panel in en-US.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchAimM3EnUs);

// Enables AIM follow ups with the Lens overlay results side panel if the AIM
// Eligibility Service indicates the user is eligible.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchAimM3UseAimEligibility);

// Enables the Lens button in the AIM Searchbox for reinvocation of selection
// overlay.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchReinvocationAffordance);

// Enables overriding the Lens overlay entrypoint label with an alternate
// string.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayEntrypointLabelAlt);

// Enables making the text selection context menu option a Lens overlay
// entrypoint.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayTextSelectionContextMenuEntrypoint);

// Force Lens overlay invocations to perform an empty CSB query. For internal
// debugging only.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayForceEmptyCsbQuery);

// Enables using a webview for the results frame instead of an iframe.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSidePanelEnableWebviewResults);

// Enables AIM suggestions in the composebox.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensAimSuggestions);

// Enables configuring the gradient background for AIM suggestions.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensAimSuggestionsGradientBackground);

// Enables the zero state contextual searchbox feature which opens the SRP
// immediately when entering Lens entry points.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchZeroStateCsb);

// Enables handling for the video citations feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensVideoCitations);

// Enables the updated feedback entrypoint in the Lens side panel. This differs
// from the "kLensSearchSidePanelNewFeedback" because this does not add a new
// entrypoint, but updates the existing one.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensUpdatedFeedbackEntrypoint);

// Enables using the optimization filter for triggering the action chip.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayOptimizationFilter);

// Enables using the non-blocking privacy notice for the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayNonBlockingPrivacyNotice);

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

// Enable "open in new tab" option in side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kLensOverlayEnableOpenInNewTab;

// Whether the EDU action chip should be disabled by glic.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kLensOverlayEduActionChipDisabledByGlic;

// Value representing the string to use to override the Lens overlay entrypoint
// label.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<int> kLensOverlayEntrypointLabelAltId;

// Whether the Lens overlay text selection context menu entrypoint should
// issue contextual queries. If false, contextualization will be suppressed for
// all queries in the session.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool>
    kLensOverlayTextSelectionContextMenuEntrypointContextualize;

// The URL for the Lens home page.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetHomepageURLForLens();

// Returns whether to apply fix for HTML redirects.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLensHtmlRedirectFix();

// Returns whether the Search Image button in the Chrome Screenshot Sharing
// feature is enabled
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensInScreenshotSharingEnabled();

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

// Returns the finch configured endpoint URL for the cluster info request.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayClusterInfoEndpointUrl();

// Returns whether or not to send the search session, visual
// search request id, and visual interaction type in suggest requests from the
// Lens search box. These params replace the existing "iil" image signals
// param.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlaySendLensInputsForLensSuggest();

// Returns whether or not to send the visual search interaction data
// in suggest requests from the Lens search box.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlaySendLensVisualInteractionDataForLensSuggest();

// Returns whether or not to send the image signals in suggest requests from
// the Lens search box.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlaySendImageSignalsForLensSuggest();

// Returns whether or not to send the vit as image data in suggest requests
// from the Lens search box.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlaySendVitAsImageForLensSuggest();

// Returns the max number of bytes to allow for content uploads.
COMPONENT_EXPORT(LENS_FEATURES)
extern uint32_t GetLensOverlayFileUploadLimitBytes();

// Returns the number of characters to be retrieved from the PDF for generating
// suggestions. This is a target and not a hard limit. The actual number of
// rounded to the nearest page. The actual number of characters may also be
// less than this value if the PDF is too small.
COMPONENT_EXPORT(LENS_FEATURES)
extern uint32_t GetLensOverlayPdfSuggestCharacterTarget();

// Returns the number of characters that should be present per page if the PDF
// is not scanned. This value is compared to the average number of characters
// per page to determine if the PDF is scanned.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetScannedPdfCharacterPerPageHeuristic();

// Returns whether to include the inner text from the underlying page in the
// request to be used as page context. This is for webpages and sends text
// equivalent to document.body.innerText. Must have UseUpdatedContextFields
// enabled when combined with other page content types.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseInnerTextAsContext();

// Returns whether to include the Annotated Page Content from the underlying
// page in the inner HTML requests used as page context. Must have
// UseUpdatedContextFields enabled when combined with other page content types.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseApcAsContext();

// Returns whether to include the page URL in the page content upload request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool SendPageUrlForContextualization();

// Returns whether to include the page title in the page content upload request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool SendPageTitleForContextualization();

// The timeout set for page content upload requests in milliseconds.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayPageContentRequestTimeoutMs();

// Returns the margin in pixels to add to the top and bottom of word bounding
// boxes.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayVerticalTextMargin();

// Returns the margin in pixels to add to the left and right of word bounding
// boxes.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayHorizontalTextMargin();

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

// Enables our blur layer.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlayUseBlur();

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

// Whether to enable the "open in new tab" option in the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlaySidePanelOpenInNewTabEnabled();

// Returns whether to use the new optimized request flow which makes a request
// to get the cluster info prior to uploading any image or page content bytes.
// This also decouples sending the images and page content bytes in the same
// request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayClusterInfoOptimizationEnabled();

// Whether to enable the early interaction optimization for the Lens Overlay.
// This optimization allows the interaction request to be sent before the full
// image response is received, if the cluster info is already available. This
// optimization will do nothing if the cluster info optimization is disabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayEarlyInteractionOptimizationEnabled();

// Time delay for the results trigger of the Lens Overlay HaTS survey.
COMPONENT_EXPORT(LENS_FEATURES)
extern base::TimeDelta GetLensOverlaySurveyResultsTime();

// Whether to enable a fetch to get the list of languages supported by the Lens
// server.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayTranslateLanguagesFetchEnabled();

// The translate endpoint URL for fetching supported languages.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayTranslateEndpointURL();

// Returns whether to show the ghost loader component for the contextual
// searchbox. This includes the loading indicator, the error state, and the hint
// text if the loading state is disabled via the feature flag below.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool EnableContextualSearchboxGhostLoader();

// Returns whether to show the ghost loader loading state in the contextual
// searchbox. If this is false, but the ghost loader is enabled, the ghost
// loader will still be shown on searchbox focuswith hint text instead of the
// loading indicator.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShowContextualSearchboxGhostLoaderLoadingState();

// The timeout set for autocomplete for lens searchboxes.
COMPONENT_EXPORT(LENS_FEATURES)
extern base::TimeDelta GetLensSearchboxAutocompleteTimeout();

// The timeout for receiving suggestions in the Lens composebox.
COMPONENT_EXPORT(LENS_FEATURES)
extern base::TimeDelta GetLensAimSuggestionTimeout();

// The list of source languages supported by Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayTranslateSourceLanguages();

// The list of additional target translate languages supported by Lens. To get
// the full list of supported target languages, we add this value to the list of
// source languages supported by Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayTranslateTargetLanguages();

// The timeout for resetting the cache of supported languages in the WebUI.
COMPONENT_EXPORT(LENS_FEATURES)
extern base::TimeDelta GetLensOverlaySupportedLanguagesCacheTimeoutMs();

// Returns whether to show autocomplete search suggestions in the contextual
// searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShowContextualSearchboxSearchSuggest();

// The amount of recent languages to show in the language pickers.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayTranslateRecentLanguagesAmount();

COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayRoutingInfoEnabled();

// Whether to enable the feature where side panel navigations are checked for
// text fragments and whether the highlights generated by these fragments can be
// rendered in the current open tab.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool HandleSidePanelTextDirectivesEnabled();

// The compression level to use when compressing the PDF bytes using zstd.
// Higher values mean better compression but also take longer to compress.
// See the introduction section in third_party/zstd/src/lib/zstd.h for more
// details.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetZstdCompressionLevel();

// This is a heuristic value that determines when to show the upload progress
// bar. The value is a percentage of the total page content upload that is
// received in the progress handler. If one call to the progress handler
// receives a value greater than this heuristic, the progress bar will not be
// shown. For example, if the heuristic is 0.3, and the first call to the
// progress handler receives 31% of the total page content, the progress bar
// will not be shown because it is assumed that the upload will finish quickly.
COMPONENT_EXPORT(LENS_FEATURES)
extern double GetUploadProgressBarShowHeuristic();

// Whether the contextual searchbox should be auto-focused when the overlay is
// first opened.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldAutoFocusSearchbox();

// Whether the visual selection updates are enabled. This is true if the
// visual selection updates feature flag is enabled or if the omnibox
// suggestions feature flag is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayVisualSelectionUpdatesEnabled();

// Whether to enable the border glow for the visual selection updates. Enabling
// this will disable the shimmer animation.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableBorderGlow();

// Whether to enable the gradient region stroke for the visual selection
// updates.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableGradientRegionStroke();

// Whether to enable the white region stroke for the visual selection updates.
// Note: `GetVisualSelectionUpdatesEnableGradientRegionStroke` takes precedence
// over this flag. This flag will have no effect if the gradient region stroke
// is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableWhiteRegionStroke();

// Whether to enable the region selected glow for the visual selection updates.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableRegionSelectedGlow();

// Whether to enable the gradient super G in the Lens searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableGradientSuperG();

// Whether to enable the thumbnail in the contextual searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableCsbThumbnail();

// Whether to enable the motion tweaks in the contextual searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableCsbMotionTweaks();

// Whether to enable thumbnail sizing tweaks for the visual selection updates.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableThumbnailSizingTweaks();

// Whether to hide the csb ellipsis for the visual selection updates.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesHideCsbEllipsis();

// Whether to enable close button tweaks for the visual selection updates.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetVisualSelectionUpdatesEnableCloseButtonTweaks();

// Whether to update the viewport on each contextual query.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UpdateViewportEachQueryEnabled();

// Whether to show zero prefix suggestions in the contextual searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShowContextualSearchboxZeroPrefixSuggest();

// Whether to use the updated client context.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsUpdatedClientContextEnabled();

// Whether to show open AIM search pages in the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldShowAimInSidePanel();

// Whether the AIM Searchbox is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetAimSearchboxEnabled();

// Whether the side panel ghost loader is disabled for AIM interactions.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetSidePanelGhostLoaderDisabledForAim();

// Whether the composebox should contextualize on focus.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetShouldComposeboxContextualizeOnFocus();

// Whether lens should show AIM suggestions.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetAimSuggestionsEnabled();

// Whether lens should show AIM suggestions with a gradient background.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetAimSuggestionsGradientBackgroundEnabled();

// Enum for the parameter values.
enum class LensAimSuggestionsType {
  kNone,
  kContextual,
  kMultimodal,
};

// Returns the type of AIM suggestions to show.
COMPONENT_EXPORT(LENS_FEATURES)
extern LensAimSuggestionsType GetLensAimSuggestionsType();

// Whether to enable AIM type ahead suggestions.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensAimTypeAheadSuggestionsEnabled();

// Whether to clear the vsint param from the multimodal request when there is no
// region selection.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ClearVsintWhenNoRegionSelection();

// Whether to close the overlay when the user transitions to the AIM UI.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldCloseOverlayOnAimTransition();

// Whether to enable the floating G for the header. This is a transparent G that
// will float to on top of the remotely rendered header.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableFloatingGForHeader();

// Whether to enable the client side header. This is a header that is rendered
// on the client side and takes up space from the results UI.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableClientSideHeader();

// Whether to enable the Lens button in the AIM searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLensButtonInSearchbox();

// Whether to use the alt loading hint when overlay is opened on web pages.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldUseAltLoadingHintWeb();

// Whether to use the alt loading hint when overlay is opened on pdfs.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldUseAltLoadingHintPdf();

// Whether to enable the summarize hint for contextual suggest.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldEnableSummarizeHintForContextualSuggest();

// Whether to enable upload chunking in the Lens Overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayUploadChunkingEnabled();

// Returns the max number of bytes to allow for upload chunking.
COMPONENT_EXPORT(LENS_FEATURES)
uint32_t GetLensOverlayChunkSizeBytes();

// The endpoint URL for upload chunking.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayUploadChunkEndpointURL();

// The timeout set for upload chunk requests in milliseconds.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayUploadChunkRequestTimeoutMs();

// The retry limit after a missing chunk error occurs.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayUploadChunkRetries();

// Whether to the new feedback entry point in the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSearchSidePanelNewFeedbackEnabled();

// Whether to recontextualize on each query.
COMPONENT_EXPORT(LENS_FEATURES)
bool ShouldLensOverlayRecontextualizeOnQuery();

// Whether to enable corner sliders for keyboard control.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool AreLensOverlayCornerSlidersEnabled();

// The timeout for performing a region search after a slider change event.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlaySliderChangedTimeout();

// Whether the protected page for the side panel is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
bool IsLensSearchProtectedPageEnabled();

// Whether to enable the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
bool IsLensOverlayEduActionChipEnabled();

// URL allow filters for the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayEduUrlAllowFilters();

// URL block filters for the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayEduUrlBlockFilters();

// URL path match allow filters for the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayEduUrlPathMatchAllowFilters();

// URL path match block filters for the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayEduUrlPathMatchBlockFilters();

// URL force-allowed match patterns for the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayEduUrlForceAllowedMatchPatterns();

// Hashed domain block filters for the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetLensOverlayEduHashedDomainBlockFilters();

// Whether EDU action chip should be disabled by glic.
COMPONENT_EXPORT(LENS_FEATURES)
bool IsLensOverlayEduActionChipDisabledByGlic();

// The number of times the EDU action chip can be shown.
COMPONENT_EXPORT(LENS_FEATURES)
int GetLensOverlayEduActionChipMaxShownCount();

// Whether to enable keyboard selection in the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayKeyboardSelectionEnabled();

// Whether to use alternate appearance for permission bubble.
COMPONENT_EXPORT(LENS_FEATURES)
bool IsLensOverlayPermissionBubbleAltEnabled();

// Whether to enable the not found on page toast.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSearchNotFoundOnPageToastEnabled();

// Whether straight to SRP flows are enabled in the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayStraightToSrpEnabled();

// If set, overrides the query text used in the Straight to SRP flow.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetStraightToSrpQuery();

// Whether the text selection context menu option should be a Lens overlay
// entrypoint.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayTextSelectionContextMenuEntrypointEnabled();

// Whether the Lens overlay text selection context menu entrypoint should
// issue contextual queries. If false, contextualization will be suppressed for
// all queries in the session.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayTextSelectionContextMenuEntrypointContextualized();

// Whether to force Lens overlay invocations to perform an empty CSB query. For
// internal debugging only.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayForceEmptyCsbQueryEnabled();

// Whether to use a webview for the results frame instead of an iframe.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSidePanelWebviewResultsEnabled();

// Whether to enable zero state contextual suggest in the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSearchZeroStateCsbEnabled();

// The query text to use for zero state CSB in the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetZeroStateCsbQuery();

// Whether the feature to enable the special handling for video citations is
// enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensVideoCitationsEnabled();

// Whether to enable the updated feedback entry point in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensUpdatedFeedbackEnabled();

// The timeout for showing the feedback toast in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensUpdatedFeedbackToastTimeoutMs();

// Whether to enable using the optimization filter for triggering the action
// chip.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayOptimizationFilterEnabled();

// Flag to control the type of suggestions for Lens Aim.
// Access this value using: kLensAimSuggestionsType.Get()
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<LensAimSuggestionsType> kLensAimSuggestionsType;

// String constants for LensAimSuggestionsType. These are used in the
// Field Trial configuration.
inline constexpr char kLensAimSuggestionsTypeNone[] = "None";
inline constexpr char kLensAimSuggestionsTypeContextual[] = "Contextual";
inline constexpr char kLensAimSuggestionsTypeMultimodal[] = "Multimodal";

// Returns the string representation of LensAimSuggestionsType for
// logging/telemetry.
std::string_view LensAimSuggestionModeToString(LensAimSuggestionsType type);

// Returns the number of AIM suggestions to show.
COMPONENT_EXPORT(LENS_FEATURES)
int GetLensAimSuggestionsCount();

// Whether to use the non-blocking privacy notice for the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayNonBlockingPrivacyNoticeEnabled();

}  // namespace lens::features
#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
