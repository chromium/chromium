// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/video_overlay_window_native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/views/overlay/video_overlay_window_views.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"

VideoOverlayWindowNativeWidgetMac::VideoOverlayWindowNativeWidgetMac(
    VideoOverlayWindowViews* widget)
    : NativeWidgetMac(widget) {}

VideoOverlayWindowNativeWidgetMac::~VideoOverlayWindowNativeWidgetMac() =
    default;

void VideoOverlayWindowNativeWidgetMac::PopulateCreateWindowParams(
    const views::Widget::InitParams& widget_params,
    remote_cocoa::mojom::CreateWindowParams* params) {
  params->window_class = remote_cocoa::mojom::WindowClass::kFrameless;
  params->style_mask = NSWindowStyleMaskFullSizeContentView;
}
