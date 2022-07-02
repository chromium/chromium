// Copyright 2021 The Chromium Authors. All rights reserved.
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
extern const base::Feature kLensStandalone;

// Feature that controls the compression of images before they are sent to Lens.
extern const base::Feature kLensImageCompression;

// Enables a variety of changes aimed to improve user's engagement with current
// Lens features.
extern const base::Feature kLensSearchOptimizations;

// Enables a fix to properly handle transparent images in Lens Image Search
extern const base::Feature kLensTransparentImagesFix;

// Enables a fix for cursor pointer/crosshair state over overlay on Mac.
// TODO(crbug/1266514): make default and remove feature once launched.
extern const base::FeatureParam<bool> kRegionSearchMacCursorFix;

// Enables using `Google` as the visual search provider instead of `Google
// Lens`.
extern const base::FeatureParam<bool> kUseGoogleAsVisualSearchProvider;

// Enables alternate option 1 for the Region Search context menu item text.
extern const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText1;

// Enables alternate option 2 for the Region Search context menu item text.
extern const base::FeatureParam<bool> kRegionSearchUseMenuItemAltText2;

// Enables UKM logging for the Lens Region Search feature.
extern const base::FeatureParam<bool> kEnableUKMLoggingForRegionSearch;

// Enables UKM logging for the LensStandalone feature.
extern const base::FeatureParam<bool> kEnableUKMLoggingForImageSearch;

// Enables the side panel for Lens features on Chrome where supported.
extern const base::FeatureParam<bool> kEnableSidePanelForLens;

// Enables Lens fullscreen search on Desktop platforms.
extern const base::FeatureParam<bool> kEnableFullscreenSearch;

// Returns whether to enable UKM logging for Lens Region Search feature.
extern bool GetEnableUKMLoggingForRegionSearch();

// Returns whether to enable UKM logging for LensStandalone feature.
extern bool GetEnableUKMLoggingForImageSearch();

// Returns the max pixel width/height for the image to be sent to Lens via
// region search. The images are sent at 1x as PNGs.
extern int GetMaxPixelsForRegionSearch();

// Returns the max area for the image to be sent to Lens via region search.
extern int GetMaxAreaForRegionSearch();

// Returns the max pixel width/height for the image to be sent to Lens.
extern int GetMaxPixelsForImageSearch();

// The URL for the Lens home page.
extern std::string GetHomepageURLForLens();

// Returns whether Lens fullscreen search is enabled.
extern bool IsLensFullscreenSearchEnabled();

// Returns whether to use alternative option 1 for the Region Search context
// menu item text.
extern bool UseRegionSearchMenuItemAltText1();

// Returns whether to use alternative option 2 for the Region Search context
// menu item text.
extern bool UseRegionSearchMenuItemAltText2();

// Returns whether to use `Google` as the visual search provider for all
// relevant Lens context menu strings.
extern bool UseGoogleAsVisualSearchProvider();

// Returns whether the Lens side panel is enabled.
extern bool IsLensSidePanelEnabled();

// Returns whether to send images to Lens Standalone as PNG
extern bool GetSendImagesAsPng();

}  // namespace features
}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_FEATURES_H_
