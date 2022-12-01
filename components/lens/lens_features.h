// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_FEATURES_H_
#define COMPONENTS_LENS_LENS_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace lens {
namespace features {

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

// Enables Lens integration into the Chrome screenshot sharing feature by adding
// a "Search Image" button.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensSearchImageInScreenshotSharing);

// Enables Latency logging for the LensStandalone feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableLatencyLogging);

// Enable keyboard shortcut for the Lens Region Search feature.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableRegionSearchKeyboardShortcut);

// Enable the Lens Region Search feature on the PDF viewer.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableRegionSearchOnPdfViewer);

// Enables the modification of the instruction chip UI that is presented when
// region search is opened.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensInstructionChipImprovements);

// Enables the image search side panel experience for third party default search
// engines
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kEnableImageSearchSidePanelFor3PDse);

// Enables launching the region search experience in a new tab with WebUI.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensRegionSearchStaticPage);

// Enables using more optimized image formats for Lens requests.
COMPONENT_EXPORT(LENS_FEATURES)
BASE_DECLARE_FEATURE(kLensImageFormatOptimizations);

// Enables using `Google` as the visual search provider instead of `Google
// Lens`.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseGoogleAsVisualSearchProvider;

// Enables alternate option 1 for the Region Search context menu item text.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText1;

// Enables alternate option 2 for the Region Search context menu item text.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText2;

// Enables alternate option 3 for the Region Search context menu item text.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText3;

// Enables UKM logging for the Lens Region Search feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableUKMLoggingForRegionSearch;

// Enables UKM logging for the LensStandalone feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableUKMLoggingForImageSearch;

// Enables the side panel for Lens features on Chrome where supported.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableSidePanelForLens;

// The base URL for Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<std::string> kHomepageURLForLens;

// Enable Lens HTML redirect fix.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableLensHtmlRedirectFix;

// Enables Lens fullscreen search on Desktop platforms.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnableFullscreenSearch;

// Enables using side panel in the Chrome Screenshot sharing feature integration
// instead of a new tab.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseSidePanelForScreenshotSharing;

// Forces the Chrome Screenshot sharing dialog bubble to stay open after the
// user clicks the Search Image button.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kEnablePersistentBubble;

// Enables the use of the selection with image icon when using the instruction
// chip improvements feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseSelectionIconWithImage;

// Enables the use of an alternative string for the instruction chip.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseAltChipString;

// Enables encoding to WebP for image search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseWebpInImageSearch;

// Value in range 0-100 that dictates the encoding quality for image search
// lossy formats, with 100 being the best quality.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<int> kEncodingQualityImageSearch;

// Enables encoding to WebP for region search queries. This param takes
// precedence over kUseJpegInRegionSearch.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseWebpInRegionSearch;

// Enables encoding to JPEG for region search queries. This param does
// nothing if kUseWebpInRegionSearch is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<bool> kUseJpegInRegionSearch;

// Value in range 0-100 that dictates the encoding quality for region search
// lossy formats, with 100 being the best quality.
COMPONENT_EXPORT(LENS_FEATURES)
extern const base::FeatureParam<int> kEncodingQualityRegionSearch;

// Enables Latency logging for the LensStandalone feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLatencyLogging();

// Returns whether the image search side panel is supported for third party
// default search engines
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableImageSearchUnifiedSidePanelFor3PDse();

// Returns whether to enable UKM logging for Lens Region Search feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableUKMLoggingForRegionSearch();

// Returns whether to enable UKM logging for LensStandalone feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableUKMLoggingForImageSearch();

// Returns the max pixel width/height for the image to be sent to Lens via
// region search. The images are sent at 1x as PNGs.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetMaxPixelsForRegionSearch();

// Returns the max area for the image to be sent to Lens via region search.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetMaxAreaForRegionSearch();

// Returns the max pixel width/height for the image to be sent to Lens.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetMaxPixelsForImageSearch();

// The URL for the Lens home page.
COMPONENT_EXPORT(LENS_FEATURES)
extern std::string GetHomepageURLForLens();

// Returns whether to apply fix for HTML redirects.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool GetEnableLensHtmlRedirectFix();

// Returns whether Lens fullscreen search is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensFullscreenSearchEnabled();

// Returns whether to use alternative option 1 for the Region Search context
// menu item text.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseRegionSearchMenuItemAltText1();

// Returns whether to use alternative option 2 for the Region Search context
// menu item text.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseRegionSearchMenuItemAltText2();

// Returns whether to use alternative option 3 for the Region Search context
// menu item text.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseRegionSearchMenuItemAltText3();

// Returns whether to use `Google` as the visual search provider for all
// relevant Lens context menu strings.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseGoogleAsVisualSearchProvider();

// Returns whether the Lens side panel is enabled for image search.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSidePanelEnabled();

// Returns whether the Lens side panel is enabled for region search.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensSidePanelEnabledForRegionSearch();

// Returns whether the Search Image button in the Chrome Screenshot Sharing
// feature is enabled
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensInScreenshotSharingEnabled();

// Returns whether the instruction chip improvement feature is enabled.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensInstructionChipImprovementsEnabled();

// Returns whether to use the Chrome Side Panel for the Lens integration in
// Chrome Screenshot Sharing feature
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseSidePanelForScreenshotSharing();

// Returns whether the Chrome Screenshot Sharing Bubble disappears after the
// user clicks the Search Image button
COMPONENT_EXPORT(LENS_FEATURES)
extern bool EnablePersistentBubble();

// Returns if we should use the selection with image icon instead of the default
// when using the instruction chip improvements feature.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseSelectionIconWithImage();

// Returns whether we should use an alternative instruction chip string.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool UseAltChipString();

// Returns whether we should use a WebUI static page for region search.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsLensRegionSearchStaticPageEnabled();

// Returns whether to use WebP encoding for image search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsWebpForImageSearchEnabled();

// Get the encoding quality for image search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetImageSearchEncodingQuality();

// Returns whether to use WebP encoding for region search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsWebpForRegionSearchEnabled();

// Returns whether to use JPEG encoding for region search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern bool IsJpegForRegionSearchEnabled();

// Get the encoding quality for region search queries.
COMPONENT_EXPORT(LENS_FEATURES)
extern int GetRegionSearchEncodingQuality();
}  // namespace features
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
