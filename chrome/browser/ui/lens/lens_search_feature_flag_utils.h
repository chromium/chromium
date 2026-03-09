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
bool IsLensOverlayContextualSearchboxEnabled(Profile* profile);

// Whether or not to enable the AIM M3 (side panel searchbox) experience.
bool IsAimM3Enabled(Profile* profile);

// Whether the EDU action chip is enabled and has not been shown too many times.
bool ShouldShowLensOverlayEduActionChip(Profile* profile);

// Records that the Lens Overlay EDU action chip has been shown by incrementing
// the counter and setting the last shown time.
void RecordLensOverlayEduActionChipShown(Profile* profile);

// Whether the user has granted the permissions needed for the overlay to appear
// or, if the non-blocking privacy notice is being used, for contextualization.
// The needed permissions vary depending on if the contextual searchbox is
// enabled.
bool DidUserGrantLensOverlayNeededPermissions(Profile* profile);

// Grants the permissions needed for the overlay to appear or, if the
// non-blocking privacy notice is being used, for contextualization. The granted
// permissions vary depending on if the contextual searchbox is enabled.
void GrantLensOverlayNeededPermissions(Profile* profile);

// If the non-blocking privacy notice is enabled and an impression cap is set,
// will call GrantLensOverlayNeededPermissions() after the impression cap has
// been reached and return true. Otherwise, will return the value of calling
// DidUserGrantLensOverlayNeededPermissions(). Should not be called if the
// non-blocking privacy notice is not enabled.
bool MaybeIncrementPrivacyNoticeShownCountAndGrantPermissions(Profile* profile);

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCH_FEATURE_FLAG_UTILS_
