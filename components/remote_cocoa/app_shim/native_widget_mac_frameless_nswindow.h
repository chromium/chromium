// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_FRAMELESS_NSWINDOW_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_FRAMELESS_NSWINDOW_H_

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

// Overrides contentRect <-> frameRect conversion methods to keep them equal to
// each other, even for windows that do not use NSBorderlessWindowMask. This
// allows an NSWindow to be frameless without attaining undesired side-effects
// of NSBorderlessWindowMask.
@interface NativeWidgetMacFramelessNSWindow : NativeWidgetMacNSWindow
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_FRAMELESS_NSWINDOW_H_
