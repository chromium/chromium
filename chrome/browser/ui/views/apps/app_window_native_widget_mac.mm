// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/apps/titlebar_background_view.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "extensions/browser/app_window/native_app_window.h"
#import "ui/base/cocoa/window_size_constants.h"

AppWindowNativeWidgetMac::AppWindowNativeWidgetMac(
    views::Widget* widget,
    extensions::NativeAppWindow* native_app_window)
    : NativeWidgetMac(widget), native_app_window_(native_app_window) {
}

AppWindowNativeWidgetMac::~AppWindowNativeWidgetMac() {
}

void AppWindowNativeWidgetMac::PopulateCreateWindowParams(
    const views::Widget::InitParams& widget_params,
    remote_cocoa::mojom::CreateWindowParams* params) {
  // If the window is frameless then use the frameless NSWindow sub-class.
  // Otherwise use the default window creation parameters.
  if (native_app_window_->IsFrameless()) {
    params->window_class = remote_cocoa::mojom::WindowClass::kFrameless;
    params->style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                         NSWindowStyleMaskMiniaturizable |
                         NSWindowStyleMaskResizable |
                         NSWindowStyleMaskFullSizeContentView;
  }
}

NativeWidgetMacNSWindow* AppWindowNativeWidgetMac::CreateNSWindow(
    const remote_cocoa::mojom::CreateWindowParams* params) {
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
