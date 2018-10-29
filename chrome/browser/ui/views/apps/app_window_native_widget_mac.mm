// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/apps/titlebar_background_view.h"
#include "extensions/browser/app_window/native_app_window.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget.mojom.h"
#import "ui/views_bridge_mac/native_widget_mac_nswindow.h"

AppWindowNativeWidgetMac::AppWindowNativeWidgetMac(
    views::Widget* widget,
    extensions::NativeAppWindow* native_app_window)
    : NativeWidgetMac(widget), native_app_window_(native_app_window) {
}

AppWindowNativeWidgetMac::~AppWindowNativeWidgetMac() {
}

void AppWindowNativeWidgetMac::PopulateCreateWindowParams(
    const views::Widget::InitParams& widget_params,
    views_bridge_mac::mojom::CreateWindowParams* params) {
  // If the window is frameless then use the frameless NSWindow sub-class.
  // Otherwise use the default window creation parameters.
  if (native_app_window_->IsFrameless()) {
    params->window_class = views_bridge_mac::mojom::WindowClass::kFrameless;
    params->style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                         NSWindowStyleMaskMiniaturizable |
                         NSWindowStyleMaskResizable;
    if (@available(macOS 10.10, *))
      params->style_mask |= NSWindowStyleMaskFullSizeContentView;
    else
      NOTREACHED();
  }
}

NativeWidgetMacNSWindow* AppWindowNativeWidgetMac::CreateNSWindow(
    const views_bridge_mac::mojom::CreateWindowParams* params) {
  NativeWidgetMacNSWindow* ns_window = NativeWidgetMac::CreateNSWindow(params);

  // If the window has a native or colored frame, use the same NSWindow as
  // NativeWidgetMac.
  if (!native_app_window_->IsFrameless() &&
      native_app_window_->HasFrameColor()) {
    [TitlebarBackgroundView
        addToNSWindow:ns_window
          activeColor:native_app_window_->ActiveFrameColor()
        inactiveColor:native_app_window_->InactiveFrameColor()];
  }

  return ns_window;
}
