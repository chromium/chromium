// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/views_scrollbar_bridge.h"

#import "base/mac/sdk_forward_declarations.h"

@interface ViewsScrollbarBridge ()

// Called when we receive a NSPreferredScrollerStyleDidChangeNotification.
- (void)onScrollerStyleChanged:(NSNotification*)notification;

@end

@implementation ViewsScrollbarBridge

- (instancetype)initWithDelegate:(ViewsScrollbarBridgeDelegate*)delegate {
  if ((self = [super init])) {
    delegate_ = delegate;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onScrollerStyleChanged:)
               name:NSPreferredScrollerStyleDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!delegate_);
  [super dealloc];
}

- (void)clearDelegate {
  delegate_ = nullptr;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)onScrollerStyleChanged:(NSNotification*)notification {
  if (delegate_)
    delegate_->OnScrollerStyleChanged();
}

+ (NSScrollerStyle)getPreferredScrollerStyle {
  return [NSScroller preferredScrollerStyle];
}

@end
