// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_NSWINDOW_DELEGATE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_NSWINDOW_DELEGATE_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;
}  // namespace remote_cocoa

// The delegate set on the NSWindow when a
// remote_cocoa::NativeWidgetNSWindowBridge is initialized.
REMOTE_COCOA_APP_SHIM_EXPORT
@interface ViewsNSWindowDelegate : NSObject <NSWindowDelegate> {
 @private
  raw_ptr<remote_cocoa::NativeWidgetNSWindowBridge, DanglingUntriaged>
      _parent;  // Weak. Owns this.
  base::scoped_nsobject<NSCursor> _cursor;
  absl::optional<float> _aspectRatio;
  gfx::Size _excludedMargin;

  // Only valid during a live resize.
  // Used to keep track of whether a resize is happening horizontally or
  // vertically, even if physically the user is resizing in both directions.
  // The value is significant when |_aspectRatio| is set, i.e., we are
  // responsible for maintaining the aspect ratio of the window. As the user is
  // dragging one of the corners to resize, we need the resize to be either
  // horizontal or vertical all the time, so we pick one of the directions and
  // stick to it. This is necessary to achieve stable results, because in order
  // to keep the aspect ratio fixed we override one window dimension with a
  // value computed from the other dimension.
  absl::optional<bool> _resizingHorizontally;
}

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

// Notify that the system control tint changed.
- (void)onSystemControlTintChanged:(NSNotification*)notification;

// Called on the delegate of a modal sheet when its modal session ends.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)contextInfo;

// Set the aspect ratio of the window. Window resizes will be constrained in an
// attempt to maintain the aspect ratio.
// Cocoa provides this functionality via the [NSWindow aspectRatio] property,
// but its implementation prioritizes the aspect ratio over the minimum size:
// one of the dimensions can go below the minimum size if that's what it takes
// to maintain the aspect ratio. This is inacceptable for us.
- (void)setAspectRatio:(float)aspectRatio
        excludedMargin:(const gfx::Size&)excludedMargin;

@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_NSWINDOW_DELEGATE_H_
