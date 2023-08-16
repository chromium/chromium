// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_NSWINDOW_DELEGATE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_NSWINDOW_DELEGATE_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "ui/gfx/geometry/size.h"

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;
}  // namespace remote_cocoa

// The delegate set on the NSWindow when a
// remote_cocoa::NativeWidgetNSWindowBridge is initialized.
REMOTE_COCOA_APP_SHIM_EXPORT
@interface ViewsNSWindowDelegate : NSObject <NSWindowDelegate>

// If set, the cursor set in -[NSResponder updateCursor:] when the window is
// reached along the responder chain.
@property(retain, nonatomic) NSCursor* cursor;

// Initialize with the given |parent|.
- (instancetype)initWithBridgedNativeWidget:
    (remote_cocoa::NativeWidgetNSWindowBridge*)parent;

// Notify that the window has been reordered in (or removed from) the window
// server's screen list. This is a substitute for -[NSWindowDelegate
// windowDidExpose:], which is only sent for nonretained windows (those without
// a backing store). |notification| is optional and can be set when redirecting
// a notification such as NSApplicationDidHideNotification.
- (void)onWindowOrderChanged:(NSNotification*)notification;

// Notify that the system colors changed.
- (void)onSystemColorsChanged:(NSNotification*)notification;

// Set the aspect ratio of the window. Window resizes will be constrained in an
// attempt to maintain the aspect ratio.
// Cocoa provides this functionality via the [NSWindow aspectRatio] property,
// but its implementation prioritizes the aspect ratio over the minimum size:
// one of the dimensions can go below the minimum size if that's what it takes
// to maintain the aspect ratio. This is unacceptable for us.
- (void)setAspectRatio:(float)aspectRatio
        excludedMargin:(const gfx::Size&)excludedMargin;

@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_NSWINDOW_DELEGATE_H_
