// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_TOUCH_BAR_DELEGATE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_TOUCH_BAR_DELEGATE_H_

#import <Cocoa/Cocoa.h>
#include <os/availability.h>

// Bridge delegate class for NativeWidgetMacNSWindow and
// BrowserWindowTouchBarMac.
@protocol WindowTouchBarDelegate <NSObject>

// Creates and returns a touch bar for the browser window.
- (NSTouchBar*)makeTouchBar API_AVAILABLE(macos(10.12.2));

@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_TOUCH_BAR_DELEGATE_H_
