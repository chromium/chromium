// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_OCCLUSION_CHECKER_MAC_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_OCCLUSION_CHECKER_MAC_H_

#import <AppKit/AppKit.h>

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

namespace remote_cocoa {

// Key present in the `userInfo` dictionary of synthetic
// `NSWindowDidChangeOcclusionStateNotification`s posted by
// `WindowOcclusionCheckerMac`.
REMOTE_COCOA_APP_SHIM_EXPORT extern NSString* const
    kWindowOcclusionCheckerNotificationUserInfoKey;

}  // namespace remote_cocoa

// The WindowOcclusionCheckerMac performs window occlusion checks for windows
// of similar size, a case macOS's occlusion detection system cannot handle
// (see crbug.com/40593221). It initiates these checks on window state change
// events (ordering, closing, etc.).
//
// The checker makes it easy to learn of window occlusion events from both
// macOS and from its own occlusion checks. Interested parties should register
// tracked windows via `-addTrackedWindow:` / `-removeTrackedWindow:` and watch
// for `NSWindowDidChangeOcclusionStateNotification` notifications where the
// object is the affected window and `userInfo` contains
// `remote_cocoa::kWindowOcclusionCheckerNotificationUserInfoKey` (or observe
// via `NativeWidgetNSWindowBridge::OnWindowOcclusionChanged()`).
REMOTE_COCOA_APP_SHIM_EXPORT
@interface WindowOcclusionCheckerMac : NSObject

+ (instancetype)sharedInstance;

// Returns the live shared instance if one exists, without lazily creating it.
+ (instancetype)sharedInstanceIfCreated;

// Registers or unregisters `window` for occlusion tracking. Calls are
// reference-counted (using weak `NSWindow` keys), so multiple consumers (e.g.
// `NativeWidgetNSWindowBridge` and one or more `WebContentsViewCocoa`s) can
// independently register the same `NSWindow`.
- (void)addTrackedWindow:(NSWindow*)window;
- (void)removeTrackedWindow:(NSWindow*)window;
- (BOOL)isTrackingWindow:(NSWindow*)window;

// API exposed for testing.

// Returns YES if the specified version is less than 13.0 or more than 13.2.
// Manual occlusion detection is not supported on macOS 13.0-13.2.
+ (BOOL)manualOcclusionDetectionSupportedForPackedVersion:(int)version;

// Returns YES if manual occlusion detection is supported for the current macOS.
+ (BOOL)manualOcclusionDetectionSupportedForCurrentMacOSVersion;

// Resets the state of `sharedInstance` during tests.
+ (void)resetSharedInstanceForTesting;

// Schedules an occlusion state update for all tracked windows.
- (void)scheduleOcclusionStateUpdates;

// Updates the occlusion states of all tracked windows.
- (void)performOcclusionStateUpdates;

// Returns YES if occlusion updates are scheduled.
- (BOOL)occlusionStateUpdatesAreScheduledForTesting;

@end

@interface NSWindow (WindowOcclusionCheckerMac)
@property(nonatomic, assign, getter=isOccluded) BOOL occluded;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_OCCLUSION_CHECKER_MAC_H_
