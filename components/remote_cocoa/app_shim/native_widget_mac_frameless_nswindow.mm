// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_frameless_nswindow.h"

@interface NSWindow (PrivateAPI)
+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle;
@end

@interface NativeWidgetMacFramelessNSWindowFrame
    : NativeWidgetMacNSWindowTitledFrame
@end

@implementation NativeWidgetMacFramelessNSWindowFrame
- (CGFloat)_titlebarHeight {
  return 0;
}
@end

@implementation NativeWidgetMacFramelessNSWindow

+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle {
  if ([NativeWidgetMacFramelessNSWindowFrame class]) {
    return [NativeWidgetMacFramelessNSWindowFrame class];
  }
  return [super frameViewClassForStyleMask:windowStyle];
}

@end
