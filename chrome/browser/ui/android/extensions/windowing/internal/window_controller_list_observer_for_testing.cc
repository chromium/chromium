// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/windowing/internal/window_controller_list_observer_for_testing.h"

#include "base/check_deref.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/ui/android/extensions/windowing/internal/extension_window_controller_bridge.h"

WindowControllerListObserverForTesting::WindowControllerListObserverForTesting(
    ExtensionWindowControllerBridge* bridge)
    : bridge_(CHECK_DEREF(bridge)) {}

void WindowControllerListObserverForTesting::OnWindowBoundsChanged(
    extensions::WindowController* window_controller) {
  if (window_controller == &(bridge_->extension_window_controller_)) {
    bridge_->RecordExtensionInternalEventForTesting(  // IN-TEST
        ExtensionInternalWindowEventForTesting::BOUNDS_CHANGED);
  }
}
