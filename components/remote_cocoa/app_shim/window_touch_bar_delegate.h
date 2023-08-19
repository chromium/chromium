// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_TOUCH_BAR_DELEGATE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_TOUCH_BAR_DELEGATE_H_

#import <Cocoa/Cocoa.h>

// Bridge delegate class for NativeWidgetMacNSWindow and
// BrowserWindowTouchBarMac.
@protocol WindowTouchBarDelegate <NSObject>

// Creates and returns a touch bar for the browser window.
- (NSTouchBar*)makeTouchBar;

@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_WINDOW_TOUCH_BAR_DELEGATE_H_
