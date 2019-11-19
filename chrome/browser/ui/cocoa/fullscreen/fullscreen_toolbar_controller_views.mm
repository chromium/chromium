// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller_views.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"

@implementation FullscreenToolbarControllerViews

- (id)initWithBrowserView:(BrowserView*)browserView {
  if ((self = [super initWithDelegate:self]))
    browserView_ = browserView;

  return self;
}

- (void)layoutToolbar {
  browserView_->Layout();
  [super layoutToolbar];
}

- (BOOL)isInAnyFullscreenMode {
  return browserView_->IsFullscreen();
}

- (BOOL)isFullscreenTransitionInProgress {
  auto* host =
      views::NativeWidgetMacNSWindowHost::GetFromNativeWindow([self window]);
  if (auto* bridge = host->GetInProcessNSWindowBridge())
    return bridge->in_fullscreen_transition();
  DLOG(ERROR) << "TODO(https://crbug.com/915110): Support fullscreen "
                 "transitions for RemoteMacViews PWA windows.";
  return false;
}

- (NSWindow*)window {
  NSWindow* ns_window = browserView_->GetNativeWindow().GetNativeNSWindow();
  if (!ns_view_) {
    auto* host =
        views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(ns_window);
    if (host) {
      if (auto* bridge = host->GetInProcessNSWindowBridge())
        ns_view_.reset([bridge->ns_view() retain]);
      else
        DLOG(ERROR) << "Cannot retain remote NSView.";
    }
  }
  return ns_window;
}

- (BOOL)isInImmersiveFullscreen {
  // TODO: support immersive fullscreen mode https://crbug.com/863047.
  return false;
}

@end
