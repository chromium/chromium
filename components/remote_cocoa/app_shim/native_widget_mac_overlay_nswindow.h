// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_OVERLAY_NSWINDOW_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_OVERLAY_NSWINDOW_H_

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

REMOTE_COCOA_APP_SHIM_EXPORT
@interface NativeWidgetMacOverlayNSWindow : NativeWidgetMacNSWindow
- (void)debugWithColor:(NSColor*)color;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_OVERLAY_NSWINDOW_H_
