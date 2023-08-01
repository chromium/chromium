// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/native_app_window_frame_view_mac.h"

#import <Cocoa/Cocoa.h>

#include "extensions/browser/app_window/native_app_window.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/widget/widget.h"

NativeAppWindowFrameViewMac::NativeAppWindowFrameViewMac(
    views::Widget* frame,
    extensions::NativeAppWindow* window)
    : views::NativeFrameViewMac(frame), native_app_window_(window) {}

NativeAppWindowFrameViewMac::~NativeAppWindowFrameViewMac() = default;

int NativeAppWindowFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  if (GetWidget()->IsFullscreen())
    return HTCLIENT;

  // Check for possible draggable region in the client area for the frameless
  // window.
  SkRegion* draggable_region = native_app_window_->GetDraggableRegion();
  if (draggable_region && draggable_region->contains(point.x(), point.y()))
    return HTCAPTION;

  return HTCLIENT;
}
