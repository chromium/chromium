// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CONSTANTS_H_

namespace segmentation_platform {

// Input Context keys for emphemeral IOS modules.
const char kIsNewUser[] = "is_new_user";
const char kIsSynced[] = "is_sycned";
const char kHasEnhancedSafeBrowsing[] = "has_enhanced_safe_browsing";

// Placeholder output label for segmentation model executor.
const char kPlaceholderEphemeralModuleLabel[] = "placeholder_module";

// Labels for emphemeral IOS modules.
const char kPriceTrackingNotificationPromo[] = "price_tracking_promo";
const char kTipsEphemeralModule[] = "tips_ephemeral_module";

// Commandline ASCII Switch key to indicate that the test module backend ranker
// should be used.
const char kEphemeralModuleBackendRankerTestOverride[] =
    "test-ephemeral-module-ranker";

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_CONSTANTS_H_
