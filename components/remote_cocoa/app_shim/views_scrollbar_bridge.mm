// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/views_scrollbar_bridge.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"

@interface ViewsScrollbarBridge ()

// Called when we receive a NSPreferredScrollerStyleDidChangeNotification.
- (void)onScrollerStyleChanged:(NSNotification*)notification;

@end

@implementation ViewsScrollbarBridge {
  raw_ptr<ViewsScrollbarBridgeDelegate> _delegate;  // Weak. Owns this.
}

- (instancetype)initWithDelegate:(ViewsScrollbarBridgeDelegate*)delegate {
  if ((self = [super init])) {
    _delegate = delegate;
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(onScrollerStyleChanged:)
               name:NSPreferredScrollerStyleDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_delegate);
}

- (void)clearDelegate {
  _delegate = nullptr;
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)onScrollerStyleChanged:(NSNotification*)notification {
  if (_delegate)
    _delegate->OnScrollerStyleChanged();
}

+ (NSScrollerStyle)preferredScrollerStyle {
  return NSScroller.preferredScrollerStyle;
}

@end
