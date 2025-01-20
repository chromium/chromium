// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_BROWSER_NATIVE_WIDGET_WINDOW_MAC_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_BROWSER_NATIVE_WIDGET_WINDOW_MAC_H_

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

REMOTE_COCOA_APP_SHIM_EXPORT
@interface BrowserNativeWidgetWindow : NativeWidgetMacNSWindow
// When set to `YES`, the traffic lights will always be shown. When set to `NO`,
// the traffic lights follow the default AppKit behavior.
- (void)setAlwaysShowTrafficLights:(BOOL)alwaysShow;
// A controller that keeps a small portion (0.5px) of the fullscreen AppKit
// NSWindow on screen.
// This controller is used as a workaround for an AppKit bug
// (https://crbug.com/1369643) that displays a black bar when changing a
// NSTitlebarAccessoryViewController's `fullScreenMinHeight` from zero
// to non-zero.
// Its presence also fixes a race condition in PWA fullscreen.
// `nil` unless the `kFullscreenPermanentThinController` is enabled.
// (see https://crbug.com/373722654)
@property(nonatomic)
    NSTitlebarAccessoryViewController* thinTitlebarViewController;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_BROWSER_NATIVE_WIDGET_WINDOW_MAC_H_
