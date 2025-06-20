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

// Enables the Lens overlay searchbox for omnibox suggestions. This does the
// same thing as kLensOverlayContextualSearchbox, but is used to enable the
// feature from the omnibox contextual suggestions experiment. This relies on
// the same params as kLensOverlayContextualSearchbox. This flag turns the meta
// feature on to remove the dependency between the CSB experiment and omnibox
// experiment.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayContextualSearchboxForOmniboxSuggestions);

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

// Enables the Lens overlay simplified selection flow.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlaySimplifiedSelection);

// Enables the Lens overlay visual selection updates.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayVisualSelectionUpdates);

// Enables the Lens overlay visual selection updates for omnibox suggestions.
// This does the same thing as kLensOverlayVisualSelectionUpdates, but is used
// to enable the feature from the omnibox contextual suggestions experiment.
// This relies on the same params as kLensOverlayVisualSelectionUpdates. This
// flag turns the meta feature on to remove the dependency between the visual
// selection ramp up and omnibox experiment.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayVisualSelectionUpdatesForOmniboxSuggestions);

// Enables the Lens overlay updated client context.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayUpdatedClientContext);

// Enables opening the Lens overlay MGT feature in the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayMGTInSidePanel);

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

// Enables a limited scroll to functionality to the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchSidePanelScrollToAPI);

// Enables the Lens overlay simplified selection flow.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayCornerSliders);

// Enables the protected error page in the Lens side panel.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchProtectedPage);

// Enables the EDU action chip.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensOverlayEduActionChip);

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

// Returns whether or not to send the search session and visual
// search request ids in suggest requests from the contextual
// search box.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetLensOverlaySendLensInputsForContextualSuggest();

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

// Returns the max number of bytes to allow for content uploads.
COMPONENT_EXPORT(LENS_FEATURES)
extern uint32_t GetLensOverlayFileUploadLimitBytes();

// Returns the number of characters to be retrieved from the PDF for generating
// suggestions. This is a target and not a hard limit. The actual number of
// characters returned may be more than this value since the characters are
// rounded to the nearest page. The actual number of characters may also be
// less than this value if the PDF is too small.
COMPONENT_EXPORT(LENS_FEATURES)
extern uint32_t GetLensOverlayPdfSuggestCharacterTarget();

// Returns whether to use the &vit=pdf param for the search request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UsePdfVitParam();

// Returns whether to use the &vit=wp param for the search request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseWebpageVitParam();

// Returns whether to use the PDF_QUERY interaction type for PDF queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UsePdfInteractionType();

// Returns whether to use the WEBPAGE_QUERY interaction type for webpage
// queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseWebpageInteractionType();

// Returns the number of characters that should be present per page if the PDF
// is not scanned. This value is compared to the average number of characters
// per page to determine if the PDF is scanned.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetScannedPdfCharacterPerPageHeuristic();

// Returns whether to use the new content fields when sending content data
// in the request payload.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseUpdatedContextFields();

// Returns whether to include PDFs from the underlying page in the request to be
// used as page context.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UsePdfsAsContext();

// Returns whether to include the inner text from the underlying page in the
// request to be used as page context. This is for webpages and sends text
// equivalent to document.body.innerText. Must have UseUpdatedContextFields
// enabled when combined with other page content types.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseInnerTextAsContext();

// Returns whether to include the inner html from the underlying page in the
// request to be used as page context. Must have UseUpdatedContextFields enabled
// when combined with other page content types.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseInnerHtmlAsContext();

// Returns whether to send the client context to the cluster info request for
// contextual suggest.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool SendClientContextToClusterInfoRequestForContextualSuggest();

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

// Whether to show the contextual searchbox in the Lens Overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayContextualSearchboxEnabled();

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

// Whether to enable the early StartQueryFlow optimization for the Lens Overlay.
// This optimization allows the full image request to be sent as soon as the
// screenshotted image is ready instead of waiting for all client-side
// initialization has completed.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensOverlayEarlyStartQueryFlowOptimizationEnabled();

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

// Whether to hold contextual queries until the user acknowledges the
// contextual searchbox. If this is disabled, the contextual queries will be
// sent immediately after the page content upload request is sent. If this is
// enabled, the contextual queries will be sent after the server responds to the
// page content upload request.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldHoldContextualQueriesUntilAck();

// Whether to compress the PDF bytes using zstd before sending them to the
// server.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldZstdCompressPdfBytes();

// The compression level to use when compressing the PDF bytes using zstd.
// Higher values mean better compression but also take longer to compress.
// See the introduction section in third_party/zstd/src/lib/zstd.h for more
// details.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetZstdCompressionLevel();

// Whether to show the upload progress bar in the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldShowUploadProgressBar();

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

// Whether to enable the simplified selection flow in the Lens overlay.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsSimplifiedSelectionEnabled();

// The text received timeout for the simplified selection feature. Time to wait
// for Lens text response before displaying the selected region context menu, in
// milliseconds.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetSimplifiedSelectionTextReceivedTimeout();

// The copy text received timeout for the simplified selection feature. Time to
// wait for text in the interaction response before falling back to using the
// full image response to copy text from a region.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetCopyTextReceivedTimeout();

// The translate text received timeout for the simplified selection feature.
// Time to wait for text in the interaction response before falling back to
// using the full image response to translate text from a region.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetTranslateTextReceivedTimeout();

// Whether the copy keyboard command (ex: CMD+C) should copy the selected region
// as an image or copy the text within the region when the simplified selection
// feature is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetShouldCopyAsImage();

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

// Whether to fix the request id for page content upload requests. When enabled,
// this will not increment the image upload request ID when the page content
// upload request is sent.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool PageContentUploadRequestIdFixEnabled();

// Whether to update the viewport on each contextual query.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UpdateViewportEachQueryEnabled();

// Whether to send the current page for PDFs.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool SendPdfCurrentPageEnabled();

// Whether to show zero prefix suggestions in the contextual searchbox.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShowContextualSearchboxZeroPrefixSuggest();

// Whether to use the updated client context.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsUpdatedClientContextEnabled();

// Whether to show open MGT search pages in the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool ShouldShowMGTInSidePanel();

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

// Whether to enable debug options for upload chunking.
COMPONENT_EXPORT(LENS_FEATURES)
bool IsLensOverlayUploadChunkingUseDebugOptionsEnabled();

// The timeout set for upload chunk requests in milliseconds.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetLensOverlayUploadChunkRequestTimeoutMs();

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

// Whether to enable the limited scroll-to API functionality in the side panel.
COMPONENT_EXPORT(LENS_FEATURES)
bool IsLensSearchSidePanelScrollToAPIEnabled();

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

}  // namespace lens::features
#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
