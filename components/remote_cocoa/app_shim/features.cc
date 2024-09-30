// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/features.h"

namespace remote_cocoa::features {
// Unwanted space switching can occur on macOS 13 and greater when immersive
// fullscreen is enabled (http://crbug.com/1454606). This feature
// enables/disables mitigation for the unwanted space switches. The mitigation
// makes use of undocumented methods in AppKit, which is the main reason its
// enablement is controlled by this feature flag.
// TODO(http://crbug.com/1454606): Remove this flag once the space switching
// issue is fixed in AppKit.
BASE_FEATURE(kImmersiveFullscreenSpaceSwitchMitigation,
             "ImmersiveFullscreenSpaceSwitchMitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled the overlay windows become visible, allowing for visual
// debugging of their position on screen.
BASE_FEATURE(kImmersiveFullscreenOverlayWindowDebug,
             "ImmersiveFullscreenOverlayWindowDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The close, minimize and maximize (traffic lights) buttons are always visible.
// kImmersiveFullscreenTabs must be enabled for this feature to have an effect.
BASE_FEATURE(kFullscreenAlwaysShowTrafficLights,
             "FullscreenAlwaysShowTrafficLights",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace remote_cocoa::features
