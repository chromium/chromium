// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_OCCLUSION_CHECKER_MAC_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_OCCLUSION_CHECKER_MAC_H_

#import <AppKit/AppKit.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

// The WindowOcclusionCheckerMac performs window occlusion checks
// for browser windows of similar size, a case macOS's occlusion
// detection system cannot handle (see crbug.com/40593221). It initiates
// these checks on window state change events (ordering, closing, etc.).
//
// The checker makes it easy to learn of window occlusion events from
// both macOS and from its own occlusion checks. Interested parties should
// watch for NSWindowDidChangeOcclusionStateNotification notifications
// where the object is the affected window and the userInfo contains the
// key "WindowOcclusionCheckerMac".
REMOTE_COCOA_APP_SHIM_EXPORT
@interface WindowOcclusionCheckerMac : NSObject

+ (instancetype)sharedInstance;

// Configures the embedder predicates used to determine whether `window`
// contains tracked content and whether occlusion updates may run (e.g. not
// during browser shutdown).
// TODO(crbug.com/505314004): Remove this temporary predicate registration in
// favor of explicit window tracking (-addTrackedWindow: /
// -removeTrackedWindow:).
+ (void)setWindowPredicate:(BOOL (*)(NSWindow*))windowPredicate
    canPerformUpdatesPredicate:(BOOL (*)())canPerformUpdatesPredicate;

// API exposed for testing.

// Returns YES if the specified version is less than 13.0 or more than 13.2.
// Manual occlusion detection is not supported on macOS 13.0-13.2.
+ (BOOL)manualOcclusionDetectionSupportedForPackedVersion:(int)version;

// Returns YES if manual occlusion detection is supported for the current macOS.
+ (BOOL)manualOcclusionDetectionSupportedForCurrentMacOSVersion;

// Resets the state of `sharedInstance` during tests.
+ (void)resetSharedInstanceForTesting;

// Schedules an occlusion state update for all windows with web contentses.
- (void)scheduleOcclusionStateUpdates;

// Updates the occlusion states of all windows with web contentses.
- (void)performOcclusionStateUpdates;

// Returns YES if occlusion updates are scheduled.
- (BOOL)occlusionStateUpdatesAreScheduledForTesting;

@end

@interface NSWindow (WindowOcclusionCheckerMac)
@property(nonatomic, assign, getter=isOccluded) BOOL occluded;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_OCCLUSION_CHECKER_MAC_H_
