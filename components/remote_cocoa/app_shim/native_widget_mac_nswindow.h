// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
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

// Selector passed to [NSApp beginSheet:]. Forwards to [self delegate], if set.
- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)contextInfo;

// Set a WindowTouchBarDelegate to allow creation of a custom TouchBar when
// AppKit follows the responder chain and reaches the NSWindow when trying to
// create one.
- (void)setWindowTouchBarDelegate:(id<WindowTouchBarDelegate>)delegate;

// Identifier for the NativeWidgetMac from which this window was created. This
// may be used to look up the NativeWidgetMacNSWindowHost in the browser process
// or the NativeWidgetNSWindowBridge in a display process.
@property(assign, nonatomic) uint64_t bridgedNativeWidgetId;

// The NativeWidgetNSWindowBridge that this will use to call back to the host.
@property(assign, nonatomic) remote_cocoa::NativeWidgetNSWindowBridge* bridge;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_H_
