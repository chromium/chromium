// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_FEATURES_H_
#define COMPONENTS_LENS_LENS_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace lens {
namespace features {

// Enables context menu search by image sending to the Lens homepage.
BASE_DECLARE_FEATURE(kLensStandalone);

// Feature that controls the compression of images before they are sent to Lens.
BASE_DECLARE_FEATURE(kLensImageCompression);

// Enables a variety of changes aimed to improve user's engagement with current
// Lens features.
BASE_DECLARE_FEATURE(kLensSearchOptimizations);

// Enables a fix to properly handle transparent images in Lens Image Search
BASE_DECLARE_FEATURE(kLensTransparentImagesFix);

// Enables Lens integration into the Chrome screenshot sharing feature by adding
// a "Search Image" button.
BASE_DECLARE_FEATURE(kLensSearchImageInScreenshotSharing);

// Enables Latency logging for the LensStandalone feature.
BASE_DECLARE_FEATURE(kEnableLatencyLogging);

// Enable the Lens Region Search feature on the PDF viewer.
BASE_DECLARE_FEATURE(kEnableRegionSearchOnPdfViewer);

// Enables the modification of the instruction chip UI that is presented when
// region search is opened.
BASE_DECLARE_FEATURE(kLensInstructionChipImprovements);

// Enables the image search side panel experience for third party default search
// engines
BASE_DECLARE_FEATURE(kEnableImageSearchSidePanelFor3PDse);

// Enables using `Google` as the visual search provider instead of `Google
// Lens`.
extern const base::FeatureParam<bool> kUseGoogleAsVisualSearchProvider;

// Enables alternate option 1 for the Region Search context menu item text.
extern const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText1;

// Enables alternate option 2 for the Region Search context menu item text.
extern const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText2;

// Enables alternate option 3 for the Region Search context menu item text.
extern const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText3;

// Enables UKM logging for the Lens Region Search feature.
extern const base::FeatureParam<bool> kEnableUKMLoggingForRegionSearch;

// Enables UKM logging for the LensStandalone feature.
extern const base::FeatureParam<bool> kEnableUKMLoggingForImageSearch;

// Enables the side panel for Lens features on Chrome where supported.
extern const base::FeatureParam<bool> kEnableSidePanelForLens;

// The base URL for Lens.
extern const base::FeatureParam<std::string> kHomepageURLForLens;

// Enable Lens HTML redirect fix.
extern const base::FeatureParam<bool> kEnableLensHtmlRedirectFix;

// Enables footer for the unified side panel
BASE_DECLARE_FEATURE(kLensUnifiedSidePanelFooter);

// Enables Lens fullscreen search on Desktop platforms.
extern const base::FeatureParam<bool> kEnableFullscreenSearch;

// Enables using side panel in the Chrome Screenshot sharing feature integration
// instead of a new tab.
extern const base::FeatureParam<bool> kUseSidePanelForScreenshotSharing;

// Forces the Chrome Screenshot sharing dialog bubble to stay open after the
// user clicks the Search Image button.
extern const base::FeatureParam<bool> kEnablePersistentBubble;

// Enables the use of the selection with image icon when using the instruction
// chip improvements feature.
extern const base::FeatureParam<bool> kUseSelectionIconWithImage;

// Enables the use of an alternative string for the instruction chip.
extern const base::FeatureParam<bool> kUseAltChipString;

// Enables Latency logging for the LensStandalone feature.
extern bool GetEnableLatencyLogging();

// Returns whether the image search side panel is supported for third party
// default search engines
extern bool GetEnableImageSearchUnifiedSidePanelFor3PDse();

// Returns whether to enable UKM logging for Lens Region Search feature.
extern bool GetEnableUKMLoggingForRegionSearch();

// Returns whether to enable UKM logging for LensStandalone feature.
extern bool GetEnableUKMLoggingForImageSearch();

// Returns whether to enable footer for lens in the unified side panel
extern bool GetEnableLensSidePanelFooter();

// Returns the max pixel width/height for the image to be sent to Lens via
// region search. The images are sent at 1x as PNGs.
extern int GetMaxPixelsForRegionSearch();

// Returns the max area for the image to be sent to Lens via region search.
extern int GetMaxAreaForRegionSearch();

// Returns the max pixel width/height for the image to be sent to Lens.
extern int GetMaxPixelsForImageSearch();

// The URL for the Lens home page.
extern std::string GetHomepageURLForLens();

// Returns whether to apply fix for HTML redirects.
extern bool GetEnableLensHtmlRedirectFix();

// Returns whether Lens fullscreen search is enabled.
extern bool IsLensFullscreenSearchEnabled();

// Returns whether to use alternative option 1 for the Region Search context
// menu item text.
extern bool UseRegionSearchMenuItemAltText1();

// Returns whether to use alternative option 2 for the Region Search context
// menu item text.
extern bool UseRegionSearchMenuItemAltText2();

// Returns whether to use alternative option 3 for the Region Search context
// menu item text.
extern bool UseRegionSearchMenuItemAltText3();

// Returns whether to use `Google` as the visual search provider for all
// relevant Lens context menu strings.
extern bool UseGoogleAsVisualSearchProvider();

// Returns whether the Lens side panel is enabled for image search.
extern bool IsLensSidePanelEnabled();

// Returns whether the Lens side panel is enabled for region search.
extern bool IsLensSidePanelEnabledForRegionSearch();

// Returns whether to send images to Lens Standalone as PNG
extern bool GetSendImagesAsPng();

// Returns whether the Search Image button in the Chrome Screenshot Sharing
// feature is enabled
extern bool IsLensInScreenshotSharingEnabled();

// Returns whether the instruction chip improvement feature is enabled.
extern bool IsLensInstructionChipImprovementsEnabled();

// Returns whether to use the Chrome Side Panel for the Lens integration in
// Chrome Screenshot Sharing feature
extern bool UseSidePanelForScreenshotSharing();

// Returns whether the Chrome Screenshot Sharing Bubble disappears after the
// user clicks the Search Image button
extern bool EnablePersistentBubble();

// Returns if we should use the selection with image icon instead of the default
// when using the instruction chip improvements feature.
extern bool UseSelectionIconWithImage();

// Returns whether we should use an alternative instruction chip string.
extern bool UseAltChipString();
}  // namespace features
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
