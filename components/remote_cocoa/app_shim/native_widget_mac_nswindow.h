// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_H_

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#import "ui/base/cocoa/command_dispatcher.h"

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;
}  // namespace remote_cocoa

@protocol WindowTouchBarDelegate;

// Weak lets Chrome launch even if a future macOS doesn't have the below classes
WEAK_IMPORT_ATTRIBUTE
@interface NSNextStepFrame : NSView
@end

WEAK_IMPORT_ATTRIBUTE
@interface NSThemeFrame : NSView
@end

REMOTE_COCOA_APP_SHIM_EXPORT
@interface NativeWidgetMacNSWindowBorderlessFrame : NSNextStepFrame
@end

REMOTE_COCOA_APP_SHIM_EXPORT
@interface NativeWidgetMacNSWindowTitledFrame : NSThemeFrame
@end

// The NSWindow used by BridgedNativeWidget. Provides hooks into AppKit that
// can only be accomplished by overriding methods.
REMOTE_COCOA_APP_SHIM_EXPORT
@interface NativeWidgetMacNSWindow : NSWindow <CommandDispatchingWindow>

// Set a CommandDispatcherDelegate, i.e. to implement key event handling.
- (void)setCommandDispatcherDelegate:(id<CommandDispatcherDelegate>)delegate;

// Set a WindowTouchBarDelegate to allow creation of a custom TouchBar when
// AppKit follows the responder chain and reaches the NSWindow when trying to
// create one.
- (void)setWindowTouchBarDelegate:(id<WindowTouchBarDelegate>)delegate;

// Enforce that this window never be made visible. In the event that it is made
// visible, it will log a crash report.
// https://crbug.com/960904
- (void)enforceNeverMadeVisible;

// `- [NSWindow orderWindow:]` does not have any effect when called on child
// windows. If you need to order a child window, use this method. Important:
// this method adds or removes children from the parent. If you're observing
// events related to adding or removing children, this could lead to issues. To
// check whether ordering is currently in progress, inspect the
// `isShufflingForOrdering` property on the child window.
- (void)orderWindowByShuffling:(NSWindowOrderingMode)place
                    relativeTo:(NSInteger)otherWin;

// Order the window to the front (space switch if necessary), and ensure that
// the window maintains its key state. A space switch will normally activate a
// window, so this function prevents that if the window is currently inactive.
- (void)orderFrontKeepWindowKeyState;

// Overridden to prevent headless windows to be constrained to the physical
// screen bounds.
- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)screen;

// Is the window a part of a browser window tree that is currently in an
// immersive fullscreen session.
- (BOOL)immersiveFullscreen;

// The sheet parent that should be used. In immersive fullscreen the preferred
// sheet parent is the root window (the browser window).
- (NSWindow*)preferredSheetParent;

// Identifier for the NativeWidgetMac from which this window was created. This
// may be used to look up the NativeWidgetMacNSWindowHost in the browser process
// or the NativeWidgetNSWindowBridge in a display process.
@property(assign, nonatomic) uint64_t bridgedNativeWidgetId;

// The NativeWidgetNSWindowBridge that this will use to call back to the host.
@property(assign, nonatomic) remote_cocoa::NativeWidgetNSWindowBridge* bridge;

// Whether this window functions as a tooltip.
@property(assign, nonatomic) BOOL isTooltip;

// Whether this window is headless.
@property(assign, nonatomic) BOOL isHeadless;

// Whether this window is currently being added to and removed from parent for
// ordering.
@property(assign, nonatomic) BOOL isShufflingForOrdering;

// Called whenever a child window is added to the receiver.
@property(nonatomic, copy) void (^childWindowAddedHandler)(NSWindow* child);

// Called whenever a child window is removed to the receiver.
@property(nonatomic, copy) void (^childWindowRemovedHandler)(NSWindow* child);

// Window to dispatch commands to. Needed for situations where the window that
// needs to handle events is not the target's immediate parent; for example
// alerts in immersive fullscreen.
@property(nonatomic, weak)
    NSWindow<CommandDispatchingWindow>* commandDispatchParentOverride;

@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_H_
