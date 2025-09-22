// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/native_app_window_frame_view_mac_client.h"

#include <optional>

#include "extensions/browser/app_window/native_app_window.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"

NativeAppWindowFrameViewMacClient::NativeAppWindowFrameViewMacClient(
    views::Widget* widget,
    extensions::NativeAppWindow* window)
    : widget_(widget), native_app_window_(window) {}

NativeAppWindowFrameViewMacClient::~NativeAppWindowFrameViewMacClient() =
    default;

std::optional<int> NativeAppWindowFrameViewMacClient::NonClientHitTest(
    const gfx::Point& point) {
  if (widget_->IsFullscreen()) {
    return HTCLIENT;
  }

  // Check for possible draggable region in the client area for the frameless
  // window.
  SkRegion* draggable_region = native_app_window_->GetDraggableRegion();
  if (draggable_region && draggable_region->contains(point.x(), point.y())) {
    return HTCAPTION;
  }

  return HTCLIENT;
}
