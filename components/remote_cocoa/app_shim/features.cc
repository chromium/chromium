// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/features.h"

namespace remote_cocoa::features {
// Unwanted space switching can occur on macOS when immersive fullscreen is
// enabled (https://crbug.com/40272387). This feature enables/disables
// mitigation for the unwanted space switches. The mitigation makes use of
// undocumented methods in AppKit, which is the main reason its enablement is
// controlled by this feature flag. TODO(https://crbug.com/40272387): Remove
// this flag once the space switching issue is fixed in AppKit.
BASE_FEATURE(kImmersiveFullscreenSpaceSwitchMitigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled the overlay windows become visible, allowing for visual
// debugging of their position on screen.
BASE_FEATURE(kImmersiveFullscreenOverlayWindowDebug,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Immersive fullscreen v2. When enabled, top chrome (tab strip, toolbar,
// bookmarks bar) stays in the browser window in immersive fullscreen, instead
// of being moved into overlay widgets that AppKit reparents into its
// NSToolbarFullScreenWindow. The NSToolbarFullScreenWindow, which hosts the
// window controls, is made transparent, and top chrome follows it as it slides
// in and out with the menu bar.
BASE_FEATURE(kMacImmersiveFullscreenV2, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSuppressAppKitFrameAdjustmentsDuringMoveLoop,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, file dialogs opened from companion or auxiliary windows
// (windows with NSWindowCollectionBehaviorFullScreenAuxiliary) are presented
// as sheets attached to the parent window. This allows the file chooser to
// display reliably when floating over native macOS fullscreen spaces.
BASE_FEATURE(kMacFullScreenAuxiliaryFileDialog,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for clearing the first responder before hiding a WebContents in
// the NSToolbarFullScreenWindow. See https://crbug.com/573809833.
BASE_FEATURE(kClearFirstResponderInToolbarFullScreenWindow,
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, causes -[CrApplication sendEvent:] to manually forward KeyUp
// events that have the command modifier to the application's key window instead
// of to |super|. Used as a killswitch if this has unintended consequences.
BASE_FEATURE(kForwardCmdKeyUpEventsToWindow, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, `NativeWidgetNSWindowBridge` registers all top-level
// (non-child, non-tooltip) Views windows with `WindowOcclusionCheckerMac`,
// rather than only tracking windows that contain a `WebContentsViewCocoa`.
BASE_FEATURE(kTrackAllTopLevelWindowsForOcclusion,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace remote_cocoa::features
