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
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_BROWSER_NATIVE_WIDGET_WINDOW_MAC_H_
