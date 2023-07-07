// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_SCROLLBAR_BRIDGE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_SCROLLBAR_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

// The delegate set to ViewsScrollbarBridge.
class REMOTE_COCOA_APP_SHIM_EXPORT ViewsScrollbarBridgeDelegate {
 public:
  // Invoked by ViewsScrollbarBridge when the system informs the process that
  // the preferred scroller style has changed
  virtual void OnScrollerStyleChanged() = 0;
};

// A bridge to NSScroller managed by NativeCocoaScrollbar. Serves as a helper
// class to bridge NSScroller notifications and functions to CocoaScrollbar.
REMOTE_COCOA_APP_SHIM_EXPORT
@interface ViewsScrollbarBridge : NSObject

// Initializes with the given delegate and registers for notifications on
// scroller style changes.
- (instancetype)initWithDelegate:(ViewsScrollbarBridgeDelegate*)delegate;

// Sets |delegate_| to nullptr.
- (void)clearDelegate;

// Returns the style of scrollers that macOS is using.
+ (NSScrollerStyle)preferredScrollerStyle;

@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_VIEWS_SCROLLBAR_BRIDGE_H_
