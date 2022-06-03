// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/native_window_tracker_cocoa.h"

#import <AppKit/AppKit.h>

@interface BridgedNativeWindowTracker : NSObject {
 @private
  NSWindow* _window;
}

- (instancetype)initWithNSWindow:(NSWindow*)window;
- (bool)wasNSWindowClosed;
- (void)onWindowWillClose:(NSNotification*)notification;

@end

@implementation BridgedNativeWindowTracker

- (instancetype)initWithNSWindow:(NSWindow*)window {
  _window = window;
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(onWindowWillClose:)
                 name:NSWindowWillCloseNotification
               object:_window];
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (bool)wasNSWindowClosed {
  return _window == nil;
}

- (void)onWindowWillClose:(NSNotification*)notification {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowWillCloseNotification
              object:_window];
  _window = nil;
}

@end

NativeWindowTrackerCocoa::NativeWindowTrackerCocoa(
    gfx::NativeWindow native_window) {
  NSWindow* window = native_window.GetNativeNSWindow();
  bridge_.reset([[BridgedNativeWindowTracker alloc] initWithNSWindow:window]);
}

NativeWindowTrackerCocoa::~NativeWindowTrackerCocoa() {
}

bool NativeWindowTrackerCocoa::WasNativeWindowClosed() const {
  return [bridge_ wasNSWindowClosed];
}

// static
std::unique_ptr<NativeWindowTracker> NativeWindowTracker::Create(
    gfx::NativeWindow window) {
  return std::unique_ptr<NativeWindowTracker>(
      new NativeWindowTrackerCocoa(window));
}
