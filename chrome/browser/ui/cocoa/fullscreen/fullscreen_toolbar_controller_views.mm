// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller_views.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/cocoa/bridged_native_widget_host_impl.h"
#include "ui/views_bridge_mac/bridged_native_widget_impl.h"

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
  views::BridgedNativeWidgetHostImpl* bridge_host =
      views::BridgedNativeWidgetHostImpl::GetFromNativeWindow([self window]);
  if (bridge_host) {
    if (bridge_host->bridge_impl())
      return bridge_host->bridge_impl()->in_fullscreen_transition();
    else
      DLOG(ERROR) << "Cannot query remote NSWindow fullscreen status.";
  }
  return bridge_host->bridge_impl()->in_fullscreen_transition();
}

- (NSWindow*)window {
  NSWindow* ns_window = browserView_->GetNativeWindow().GetNativeNSWindow();
  if (!ns_view_) {
    views::BridgedNativeWidgetHostImpl* bridge_host =
        views::BridgedNativeWidgetHostImpl::GetFromNativeWindow(ns_window);
    if (bridge_host) {
      if (bridge_host->bridge_impl())
        ns_view_.reset([bridge_host->bridge_impl()->ns_view() retain]);
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
