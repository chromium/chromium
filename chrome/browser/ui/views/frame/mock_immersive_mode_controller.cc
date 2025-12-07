// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/mock_immersive_mode_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

MockImmersiveModeController::MockImmersiveModeController(
    BrowserWindowInterface* browser)
    : ImmersiveModeController(browser) {}

MockImmersiveModeController::~MockImmersiveModeController() {
  for (auto& observer : observers_) {
    observer.OnImmersiveModeControllerDestroyed();
  }
}
