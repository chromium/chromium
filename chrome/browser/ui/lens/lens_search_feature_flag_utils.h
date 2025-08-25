// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCH_FEATURE_FLAG_UTILS_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCH_FEATURE_FLAG_UTILS_

#include "chrome/browser/profiles/profile.h"

// Utility functions for checking if Lens Search features are enabled. Separated
// from lens_features.h to allow dependencies on chrome/browser.
namespace lens {

// Whether to show the contextual searchbox in the Lens Overlay.
bool IsLensOverlayContextualSearchboxEnabled();

// Whether or not to enable the AIM M3 (side panel searchbox) experience.
bool IsAimM3Enabled(Profile* profile);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_FEATURE_FLAG_UTILS_
