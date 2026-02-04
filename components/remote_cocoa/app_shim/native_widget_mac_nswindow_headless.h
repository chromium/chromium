// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_HEADLESS_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_HEADLESS_H_

#include <optional>

#include "ui/gfx/geometry/rect.h"

// Holds headless mode related info for NSWindow.
struct NativeWidgetMacNSWindowHeadlessInfo {
  NativeWidgetMacNSWindowHeadlessInfo();

  enum class WindowState {
    kNormal,
    kZoomed,
    kFullscreen,
    kMiniaturized,
  } window_state = WindowState::kNormal;

  bool is_visible = false;
  bool is_key = false;
  std::optional<gfx::Rect> restored_bounds;
};

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NATIVE_WIDGET_MAC_NSWINDOW_HEADLESS_H_
